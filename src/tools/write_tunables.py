# writes the SPSA output into the tunables.h
import re
from pathlib import Path

TUNABLE_PATH = Path(__file__).parents[1] / "Raphael" / "tunable.h"
TUNABLE_PATTERN = re.compile(r"(Tunable(?:Callback)?)\(([A-Z0-9_]+), (-?\d+), (.*)\);")


if __name__ == "__main__":
    name_value_pairs: dict[str, str] = {}

    print("Enter SPSA Output:")
    while spsa_output := input():
        if not spsa_output.strip():
            break
        pair = spsa_output.split(", ")
        name_value_pairs[pair[0]] = pair[1]

    deltas: dict[str, float] = {}

    def replace(match: re.match) -> bool:
        tunable_type = match.group(1)
        name = match.group(2)
        old_value = float(match.group(3))
        rest = match.group(4)

        if name in name_value_pairs:
            new_value = name_value_pairs[name]
            delta = (float(new_value) - old_value) / abs(old_value)
            deltas[name] = delta

            return f"{tunable_type}({name}, {new_value}, {rest});"
        else:
            return match.group(0)

    # replace all tunables with their new values
    with open(TUNABLE_PATH, "r") as f:
        content = f.read()

    content = TUNABLE_PATTERN.sub(replace, content)

    missing = set(name_value_pairs.keys()) - set(deltas.keys())
    if missing:
        raise ValueError(f"Could not find tunables: {missing}")

    with open(TUNABLE_PATH, "w") as f:
        f.write(content)

    # rank the tunables by their delta
    ranked = sorted(deltas.items(), key=lambda x: abs(x[1]), reverse=True)
    print("Tunable Deltas:")
    for name, delta in ranked:
        print(f"{name}: {delta:.2%}")
