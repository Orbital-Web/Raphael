# script to help edit NNUE/history.txt
from pathlib import Path

HISTORY_PATH = Path(__file__).parents[1] / "NNUE" / "history.txt"


def _read_history() -> list[str]:
    with HISTORY_PATH.open("r", encoding="utf-8") as f:
        data = f.readlines()
    return data


def _write_history(new_history: str) -> None:
    with HISTORY_PATH.open("w", encoding="utf-8") as f:
        f.write(new_history)


def _find_at(history: list[str]) -> tuple[int, int, int, int]:
    """Finds the bounds of the @ symbol cell

    Args:
        history: the content of the history file

    Returns:
        [r0, c0, r1, c1]: a tuple with (r0, c0) being the coordinate of the '@' symbol,
        and (r1, c1) being the bottom right corner of the cell the symbol resides in
    """
    region = 0  # 0 - before @, 1 - after @ in the cell, 2 - after @ out the cell
    r0, c0, r1, c1 = (0, 0, 0, 0)

    for row, line in enumerate(history):
        split = line.split("@")

        if len(split) == 1:
            if region == 1:
                if line[c0].startswith("-"):
                    region = 2
                else:
                    r1 = row
            continue

        if len(split) != 2 or region != 0:
            print(split)
            raise ValueError(f"Found multiple '@' in line {row + 1}")
        before, after = split
        cell, *_ = after.split("|", 1)
        region = 1

        r0 = row
        c0 = len(before)
        c1 = c0 + len(cell)

    if region == 0:
        raise ValueError("Could not find '@'")

    return (r0, c0, r1, c1)


def extend() -> None:
    history = _read_history()
    (_, _, _, c1) = _find_at(history)

    ext = int(input("How many columns to extend (negative to shrink)?: "))
    if c1 + ext <= 0:
        raise ValueError(f"You cannot shrink the column width to 0")

    started = False
    new_history: str = ""

    for line in history:
        if line.startswith("NET/DATA"):
            started = True
        if not started:
            new_history += line
            continue

        if ext > 0:
            before, after = line[:c1], line[c1:]
            pad = "-" if line.startswith("-") else " "
            new_history += before + (pad * ext) + after
        else:
            before, after = line[: c1 + ext], line[c1:]
            new_history += before + after

    _write_history(new_history)


def insert() -> None:
    history = _read_history()
    (r0, c0, r1, c1) = _find_at(history)

    content: list[str] = []
    print("Enter content to insert:")
    while cont := input():
        if not cont.strip():
            break
        content.append(cont.rstrip())

    new_history: str = "".join(history[:r0])

    # TODO: auto line break free-form text
    for row, (line, cont) in enumerate(zip(history[r0:], content), r0):
        if row > r1:
            break

        before, after = line[:c0], line[c1 + 1 :]
        new_history += before + cont.ljust(c1 - c0 + 1) + after

    new_history += "".join(history[row + 1 :])

    _write_history(new_history)


if __name__ == "__main__":
    print("Select an operation: ")
    print("  e (extend)     - expands or shrinks the column containing the @")
    print("  i (insert)     - inserts a block of text starting at the @")
    print("")
    op = input("Operation: ")

    if op.lower() == "e":
        extend()
    elif op.lower() == "i":
        insert()
    else:
        raise ValueError(f"Unknown operation: {op}")
