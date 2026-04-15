# script to help edit NNUE/history.txt
from pathlib import Path
from typing import TypedDict

HISTORY_PATH = Path(__file__).parents[1] / "NNUE" / "history.txt"
START_KEYWORD = "NET/DATA ID"


class History(TypedDict):
    lines: list[str]
    rows: list[int]
    cols: list[int]


def _read_history() -> History:
    with HISTORY_PATH.open("r", encoding="utf-8") as f:
        lines = f.readlines()

    started = False
    rows: list[int] = []
    cols: list[int] = [0]

    for row, line in enumerate(lines):
        if line.startswith(START_KEYWORD):
            started = True
            for col, c in enumerate(line):
                if c == "|":
                    cols.append(col)

        if started and line.startswith("-"):
            rows.append(row)

    return {
        "lines": lines,
        "rows": rows,
        "cols": cols,
    }


def _write_history(new_history: str) -> None:
    with HISTORY_PATH.open("w", encoding="utf-8") as f:
        f.write(new_history)


def _find_at(history: History) -> tuple[int, int, int, int]:
    """Finds the bounds of the @ symbol cell

    Args:
        history: the content of the history file

    Returns:
        [r0, c0, r1, c1]: a tuple with (r0, c0) being the coordinate of the '@' symbol,
        and (r1, c1) being the bottom right corner of the cell the symbol resides in
    """
    found = False
    r0, c0, r1, c1 = (0, 0, 0, 0)

    for row, line in enumerate(history["lines"]):
        split = line.split("@")
        if len(split) == 1:
            continue
        if len(split) != 2 or found:
            raise ValueError(f"Found multiple '@' in line {row + 1}")

        found = True
        r0 = row
        c0 = len(split[0])
        r1 = next((r for r in history["rows"] if r > r0), -1)
        c1 = next((c for c in history["cols"] if c > c0), -1)
        assert r1 != -1
        assert c1 != -1

    if not found:
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
    for line in history["lines"]:
        if line.startswith(START_KEYWORD):
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

    raw_content = ""
    print("Enter content to insert:")
    while cont := input():
        if not cont:
            break
        raw_content += cont + "\n"

    max_width = c1 - c0 - 1
    content: list[str] = []
    for cont in raw_content.split("\n"):
        buffer, *words = cont.split(" ")
        width = len(buffer)

        for word in words:
            if width + len(word) + 1 > max_width:
                content.append(buffer)
                width = len(word)
                buffer = word
                continue
            width += len(word) + 1
            buffer += " " + word

        if buffer:
            content.append(buffer)

    print(r0, r1, max_width)
    new_history: str = "".join(history["lines"][:r0])
    for row, (line, cont) in enumerate(zip(history["lines"][r0:], content), r0):
        if row > r1:
            break

        before, after = line[:c0], line[c1:]
        new_history += before + cont.ljust(c1 - c0) + after

    new_history += "".join(history["lines"][row + 1 :])

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
