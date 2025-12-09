# a very simple script to dedupe training data across files (very heavy memory usage)
fens = set()

for i in range(1, 126):
    print(i)
    with open(f"epd/train2/{i}.epd", "w") as outf:
        with open(f"epd/train/{i}.epd", "r") as f:
            for line in f:
                if "-" in line:
                    fen, _ = line.split(" -", 1)
                else:
                    fen, _ = line.split(" [", 1)
                fen = fen.strip()
                if fen not in fens:
                    outf.write(line.strip() + "\n")
                    fens.add(fen)
