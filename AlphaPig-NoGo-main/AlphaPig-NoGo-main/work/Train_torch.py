"""
AlphaPig Training Script - PyTorch GPU version
Replaces Train.py with GPU acceleration while maintaining
binary weight format compatible with SelfPlay.exe (C++ MKL engine).

Expected format per layer in network.weights (matching C++ Network::load):
  For each Conv+BN:
    1. BN biases (beta)     - filters floats
    2. BN scales (gamma)    - filters floats  (if normalize)
    3. BN running_mean      - filters floats  (if normalize)
    4. BN running_var       - filters floats  (if normalize)
    5. Conv kernel weights  - (out*in*kh*kw) floats, Keras format: (kh,kw,in,out)->transpose->(out,in,kh,kw)

  For each Dense:
    1. Biases (zeros)       - outputs floats
    2. Weights              - (outputs*inputs) floats, Keras format: (in,out)->transpose->(out,in)
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
import random
import struct
import os

# ============================================================
# Configuration
# ============================================================
model_path = 'model_torch.pt'
weights_path = 'network.weights'
train_dir = 'dataset'
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

# ============================================================
# Model Definition (matches Keras architecture exactly)
# ============================================================

class ResidualBlock(nn.Module):
    def __init__(self, channels=64):
        super().__init__()
        self.conv1 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn1 = nn.BatchNorm2d(channels)
        self.conv2 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn2 = nn.BatchNorm2d(channels)

    def forward(self, x):
        residual = x
        out = F.relu(self.bn1(self.conv1(x)))
        out = self.bn2(self.conv2(out))
        out = F.relu(out + residual)
        return out


class AlphaPigNet(nn.Module):
    """AlphaPig NoGo network: 6-ch input -> 5 resblocks -> value head + policy head"""
    def __init__(self, in_channels=6, board_size=9, filters=64, num_blocks=5):
        super().__init__()
        self.board_size = board_size
        self.filters = filters

        # Initial conv block
        self.conv1 = nn.Conv2d(in_channels, filters, 3, padding=1, bias=False)
        self.bn1 = nn.BatchNorm2d(filters)

        # Residual blocks
        self.res_blocks = nn.ModuleList([ResidualBlock(filters) for _ in range(num_blocks)])

        # Post-residual conv block
        self.post_conv = nn.Conv2d(filters, filters, 3, padding=1, bias=False)
        self.post_bn = nn.BatchNorm2d(filters)

        # Value head
        self.value_conv = nn.Conv2d(filters, 1, 1, bias=False)
        self.value_bn = nn.BatchNorm2d(1)
        self.value_fc1 = nn.Linear(board_size * board_size, 64, bias=False)
        self.value_fc2 = nn.Linear(64, 1, bias=False)

        # Policy head
        self.policy_conv = nn.Conv2d(filters, 1, 1, bias=False)

    def forward(self, x):
        # Input: (batch, 6, 9, 9)
        x = F.relu(self.bn1(self.conv1(x)))

        for block in self.res_blocks:
            x = block(x)

        x = F.relu(self.post_bn(self.post_conv(x)))

        # Value head
        v = F.relu(self.value_bn(self.value_conv(x)))
        v = v.view(v.size(0), -1)  # flatten
        v = F.relu(self.value_fc1(v))
        v = torch.tanh(self.value_fc2(v))

        # Policy head
        p = self.policy_conv(x)
        p = p.view(p.size(0), -1)  # flatten
        p = F.softmax(p, dim=1)

        return v, p


# ============================================================
# Binary Weight Export (must match C++ Network.cpp loader exactly)
# ============================================================

def write_conv_weights(f, conv_weight):
    """
    Write Conv2D kernel weights.
    PyTorch: (out_ch, in_ch, kh, kw) -> need Keras format: (kh, kw, in_ch, out_ch) -> transpose -> (out_ch, in_ch, kh, kw)
    Since PyTorch is already (out_ch, in_ch, kh, kw), just flatten directly.
    """
    w = conv_weight.cpu().detach().numpy()
    for v in w.flatten():
        f.write(struct.pack('f', float(v)))


def write_bn_weights(f, bn_layer):
    """
    Write BatchNorm weights in order: biases(beta), scales(gamma), running_mean, running_var
    C++ reads: biases, scales, mean, stddev(=sqrt(var)+eps)
    """
    beta = bn_layer.bias.cpu().detach().numpy()      # biases
    gamma = bn_layer.weight.cpu().detach().numpy()    # scales
    mean = bn_layer.running_mean.cpu().detach().numpy()
    var = bn_layer.running_var.cpu().detach().numpy()

    for v in beta.flatten():
        f.write(struct.pack('f', float(v)))
    for v in gamma.flatten():
        f.write(struct.pack('f', float(v)))
    for v in mean.flatten():
        f.write(struct.pack('f', float(v)))
    # C++ applies sqrt(var) + epsilon, so we store raw variance (C++ does sqrt on load)
    for v in var.flatten():
        f.write(struct.pack('f', float(v)))


def write_dense_weights(f, linear_weight):
    """
    Write Dense/Linear layer weights.
    PyTorch Linear weight: (out_features, in_features) - already Keras transposed format.
    """
    w = linear_weight.cpu().detach().numpy()
    for v in w.flatten():
        f.write(struct.pack('f', float(v)))


def write_zero_biases(f, count):
    """Write zero biases for Dense layers (use_bias=False in original model)."""
    for _ in range(count):
        f.write(struct.pack('f', 0.0))


def export_weights(model, filepath):
    """
    Export model to binary format matching C++ Network.cpp loading order.
    """
    with open(filepath, 'wb') as f:
        # Header: 5 int32 zeros (20 bytes)
        f.write(struct.pack('iiiii', 0, 0, 0, 0, 0))

        # 1. Initial conv block: BN1 beta/gamma/mean/var, then Conv1 kernel
        write_bn_weights(f, model.bn1)
        write_conv_weights(f, model.conv1.weight)

        # 2-6. 5 residual blocks (each: 2 conv+bn pairs)
        for block in model.res_blocks:
            # First conv+bn of residual block (with ReLU)
            write_bn_weights(f, block.bn1)
            write_conv_weights(f, block.conv1.weight)
            # Second conv+bn of residual block (no ReLU in C++, handled by ShortcutLayer)
            write_bn_weights(f, block.bn2)
            write_conv_weights(f, block.conv2.weight)

        # 7. Post-residual conv block
        write_bn_weights(f, model.post_bn)
        write_conv_weights(f, model.post_conv.weight)

        # 8. Value head conv1x1
        write_bn_weights(f, model.value_bn)
        write_conv_weights(f, model.value_conv.weight)

        # 9. Value head Dense(81->64)
        write_zero_biases(f, 64)
        write_dense_weights(f, model.value_fc1.weight)

        # 10. Value head Dense(64->1) = valueHead
        write_zero_biases(f, 1)
        write_dense_weights(f, model.value_fc2.weight)

        # 11. Policy head conv1x1
        write_zero_biases(f, 1)
        write_conv_weights(f, model.policy_conv.weight)


# ============================================================
# Training
# ============================================================

def augment_sample(board_input, policy_target):
    """Apply random rotation and flip for data augmentation."""
    times = random.randint(1, 4)
    # Rotate
    board_input = torch.rot90(board_input, times, [1, 2])
    policy_target = torch.rot90(policy_target, times, [0, 1])
    # Flip
    if random.choice([True, False]):
        board_input = torch.flip(board_input, [1])
        policy_target = torch.flip(policy_target, [0])
    return board_input, policy_target


def main():
    print(f'Using device: {device}')
    if torch.cuda.is_available():
        print(f'GPU: {torch.cuda.get_device_name(0)}')
        print(f'VRAM: {torch.cuda.get_device_properties(0).total_memory / 1e9:.1f} GB')

    # Load or create model
    print('Loading model...')
    model = AlphaPigNet(in_channels=6, board_size=9, filters=64, num_blocks=5)
    if os.path.exists(model_path):
        model.load_state_dict(torch.load(model_path, map_location=device, weights_only=True))
        print(f'Loaded from {model_path}')
    else:
        print('Building new model...')
        torch.save(model.state_dict(), model_path)
        export_weights(model, weights_path)
        print(f'Saved initial model to {model_path} and {weights_path}')

    model = model.to(device)

    # Load training data
    print('Loading data...')
    train_files = [f for f in os.listdir(train_dir)
                   if os.path.exists(os.path.join(train_dir, f))]
    if not train_files:
        print('No training data found. Skipping training.')
        return

    all_data = []
    for file in train_files:
        data = np.loadtxt(os.path.join(train_dir, file))
        if data.ndim == 1:
            data = data.reshape(1, -1)
        all_data.append(data)
    all_data = np.concatenate(all_data, axis=0)
    print(f'Loaded {len(all_data)} training samples')

    # Setup optimizer
    optimizer = torch.optim.SGD(model.parameters(), lr=0.01, momentum=0.9, weight_decay=0.0001)
    value_criterion = nn.MSELoss()

    # Training loop
    print('Start training...')
    model.train()
    for epoch in range(4):
        # Sample subset
        indices = np.random.choice(all_data.shape[0], min(256 * 1000, all_data.shape[0]), replace=False)
        batch_data = all_data[indices]

        # Parse data
        board_input = torch.from_numpy(
            batch_data[:, :6 * 9 * 9].reshape(-1, 6, 9, 9)
        ).float()
        policy_target = torch.from_numpy(
            batch_data[:, 6 * 9 * 9:7 * 9 * 9].reshape(-1, 9, 9)
        ).float()
        value_target = torch.from_numpy(
            batch_data[:, 7 * 9 * 9:7 * 9 * 9 + 1].reshape(-1, 1)
        ).float()

        # Augment each sample
        for i in range(len(board_input)):
            board_input[i], policy_target[i] = augment_sample(board_input[i], policy_target[i])

        policy_target = policy_target.reshape(-1, 81)

        # Train in batches on GPU
        batch_size = 256
        total_loss = 0
        num_batches = 0

        for b in range(0, len(board_input), batch_size):
            bx = board_input[b:b + batch_size].to(device)
            pv = policy_target[b:b + batch_size].to(device)
            vv = value_target[b:b + batch_size].to(device)

            v_pred, p_pred = model(bx)
            loss_v = value_criterion(v_pred, vv)
            loss_p = F.cross_entropy(
                p_pred + 1e-8,  # avoid log(0)
                pv,
                reduction='mean'
            )
            # Actually, the original uses categorical_crossentropy on softmax output
            # PyTorch cross_entropy includes log_softmax internally, so use NLLLoss or KLDiv
            # Correct approach: CCE = -sum(p_target * log(p_pred))
            loss_p = -(pv * torch.log(p_pred + 1e-8)).sum(dim=1).mean()

            loss = 0.5 * loss_v + 0.5 * loss_p

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            num_batches += 1

            if num_batches % 100 == 0:
                print(f'  Epoch {epoch+1}/4, Batch {num_batches}, '
                      f'Loss: {loss.item():.4f}, V: {loss_v.item():.4f}, P: {loss_p.item():.4f}')

        print(f'Epoch {epoch+1}/4 complete, Avg Loss: {total_loss / num_batches:.4f}')

    # Save model
    print('Save model...')
    torch.save(model.state_dict(), model_path)
    export_weights(model, weights_path)
    print(f'Model saved: {model_path}, {weights_path}')
    print('Training complete!')


if __name__ == '__main__':
    main()
