# preprocesses the network file from bullet
import sys

import numpy as np

L1_SIZE = 1024
L2_SIZE = 16
L3_SIZE = 32
NUM_INPUT_BUCKET = 16
NUM_OUTPUT_BUCKET = 8

BUCKETS = [
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  8,  9,  9,
    10, 10, 11, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15
]   # fmt: skip

NETWORK: dict[str, tuple[tuple[int], np.dtype]] = {
    "l0w": ((NUM_INPUT_BUCKET, 12, 64, L1_SIZE), np.dtype(np.int16)),
    "l0b": ((L1_SIZE,), np.dtype(np.int16)),
    "l1w": ((NUM_OUTPUT_BUCKET, L2_SIZE, L1_SIZE), np.dtype(np.int8)),
    "l1b": ((NUM_OUTPUT_BUCKET, L2_SIZE), np.dtype(np.float32)),
    "l2w": ((NUM_OUTPUT_BUCKET, L3_SIZE, L2_SIZE), np.dtype(np.float32)),
    "l2b": ((NUM_OUTPUT_BUCKET, L3_SIZE), np.dtype(np.float32)),
    "l3w": ((NUM_OUTPUT_BUCKET, L3_SIZE), np.dtype(np.float32)),
    "l3b": ((NUM_OUTPUT_BUCKET,), np.dtype(np.float32)),
}


def load_network(filename: str) -> dict[str, np.ndarray]:
    assert len(set(BUCKETS)) == NUM_INPUT_BUCKET, "incorrect input bucket count"
    assert max(BUCKETS) + 1 == NUM_INPUT_BUCKET, "incorrect output bucket count"

    NET_SIZE = sum(np.prod(shape) * dt.itemsize for (shape, dt) in NETWORK.values())
    PADDING_SIZE = ((NET_SIZE + 63) // 64 * 64) - NET_SIZE

    net = {}
    with open(filename, "rb") as raw:
        for layer, (shape, dt) in NETWORK.items():
            expected = np.prod(shape) * dt.itemsize
            chunk = raw.read(expected)
            assert len(chunk) == expected, f"file terminated, could not read {layer=}"
            net[layer] = np.frombuffer(chunk, dtype=dt).reshape(shape)

        chunk = raw.read()
        assert len(chunk) == PADDING_SIZE, f"invalid network size"

    return net


def merge_king_planes(net: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
    # since the buckets are at most 2 squares in any direction, any time our king is in
    # the bucket squares, the enemy king must be outside the bucket squares
    # thus, we can copy over the enemy king features to the same plane as our king
    # features, thus reducing the network size by 64 * NUM_INPUT_BUCKET parameters
    ft = net["l0w"]
    merged_ft = ft[:, :11, :, :].copy()

    full_buckets = np.full((8, 8), -1, dtype=np.int16)
    full_buckets[:, :4] = np.array(BUCKETS).reshape((8, 4))

    OUR_KING = 5
    THEIR_KING = 11

    for bucket in range(NUM_INPUT_BUCKET):
        bucket_mask = (full_buckets == bucket).flatten()
        merged_ft[bucket, OUR_KING, :, :] = ft[bucket, THEIR_KING, :, :]
        merged_ft[bucket, OUR_KING, bucket_mask, :] = ft[
            bucket, OUR_KING, bucket_mask, :
        ]
    net["l0w"] = merged_ft

    return net


def permute_l1(net: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
    # we must put L2_SIZE * 4 tiles contiguously in memory for fast inference
    l1w = net["l1w"]  # [output][l2][l1]
    l1wp = np.empty((NUM_OUTPUT_BUCKET, L1_SIZE // 4, L2_SIZE * 4), l1w.dtype)

    for b in range(NUM_OUTPUT_BUCKET):
        for i in range(0, L1_SIZE // 4):
            l1wp[b, i, :] = l1w[b, :, 4 * i : 4 * (i + 1)].reshape(L2_SIZE * 4)
    net["l1w"] = l1wp

    return net


def permute_l2(net: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
    # we want to map [bucket][output][input] to [bucket][input][output]
    l2w = net["l2w"]
    l2wp = l2w.transpose((0, 2, 1))
    net["l2w"] = l2wp

    return net


def write_network(net: dict[str, np.ndarray], filename: str) -> None:
    filesize = 0
    with open(filename, "wb") as file:
        for layer in net.values():
            data = layer.flatten().tobytes()
            filesize += len(data)
            file.write(data)

        # write flags
        file.write(np.int8(0).tobytes())  # 0 = NONE permutation
        filesize += 1

        padding_size = ((filesize + 63) // 64 * 64) - filesize
        file.write(bytes(padding_size))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <filename>")
        sys.exit(1)

    filename = sys.argv[1]
    net = load_network(filename)

    # permuting l0 is done at build time, depending on the selected arch
    net = merge_king_planes(net)
    net = permute_l1(net)
    net = permute_l2(net)

    write_network(net, f"processed_{filename}")
