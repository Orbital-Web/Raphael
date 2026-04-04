import sys

import numpy as np

HIDDEN_SIZE = 1024
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


def load_network(filename: str) -> dict[str, np.ndarray]:
    assert len(set(BUCKETS)) == NUM_INPUT_BUCKET, "incorrect input bucket count"
    assert max(BUCKETS) + 1 == NUM_INPUT_BUCKET, "incorrect output bucket count"

    W0_SIZE = 12 * 64 * NUM_INPUT_BUCKET * HIDDEN_SIZE
    B0_SIZE = HIDDEN_SIZE
    W1_SIZE = NUM_OUTPUT_BUCKET * 2 * HIDDEN_SIZE
    B1_SIZE = NUM_OUTPUT_BUCKET

    NET_SIZE = W0_SIZE + B0_SIZE + W1_SIZE + B1_SIZE
    PADDED_NET_SIZE = (NET_SIZE + 31) // 32 * 32  # padded to 64 bytes

    data = np.frombuffer(open(filename, "rb").read(), dtype=np.int16)
    assert data.size == PADDED_NET_SIZE, "invalid network size"

    ft = data[:W0_SIZE].reshape((NUM_INPUT_BUCKET, 12, 64, HIDDEN_SIZE))
    rest = data[W0_SIZE:]

    return {"ft": ft, "rest": rest}


def merge_king_planes(net: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
    # since the buckets are at most 2 squares in any direction, any time our king is in
    # the bucket squares, the enemy king must be outside the bucket squares
    # thus, we can copy over the enemy king features to the same plane as our king
    # features, thus reducing the network size by 64 * NUM_INPUT_BUCKET parameters
    ft = net["ft"]
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

    return {"ft": merged_ft, "rest": net["rest"]}


def write_network(net: dict[str, np.ndarray], filename: str) -> None:
    data = np.concatenate([net["ft"].flatten(), net["rest"]])
    assert data.dtype == np.int16, "network data type must be int16"
    assert data.size % 32 == 0, "network size is not padded to 64 bytes"

    data.tofile(filename)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <filename>")
        sys.exit(1)

    filename = sys.argv[1]
    net = load_network(filename)

    net = merge_king_planes(net)

    write_network(net, f"processed_{filename}")
