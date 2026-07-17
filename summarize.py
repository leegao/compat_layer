import os
from collections import defaultdict

base_dir = "evaluation_results"
data = defaultdict(lambda: defaultdict(int))
gpus = defaultdict(lambda: defaultdict(set))
profile_counts = defaultdict(set)

if os.path.exists(base_dir):
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith(".txt") and file != "summary.txt":
                parts = root.split(os.sep)
                if len(parts) >= 3:
                    driver = parts[-1]
                    profile_id = file
                    profile_counts[driver].add(profile_id)

                    file_path = os.path.join(root, file)
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                        for line in f:
                            line = line.strip()
                            if "[???]" in line:
                                data[driver][line] += 1
                                gpus[driver][line].add(parts[1])
                            else:
                                # initialize data/gpus[driver]
                                if driver not in data:
                                    data[driver] = defaultdict(int)
                                if driver not in gpus:
                                    gpus[driver] = defaultdict(set)

text_output = []
for driver in sorted(data.keys(), reverse=True):
    total_profiles = len(profile_counts[driver])
    sorted_issues = sorted(data[driver].items(), key=lambda x: x[1], reverse=True)
    text_output.append(
        f"driver {driver}: {total_profiles} profiles ({len(sorted_issues)} issues)"
    )

    for issue, count in sorted_issues:
        sorted_gpus = sorted(gpus[driver][issue])
        text_output.append(f"    {issue} - {count} profiles ({', '.join(sorted_gpus)})")
    text_output.append("")

final_text = "\n".join(text_output)

markdown_output = ["### Vulkan Profiles Aggregated Unknown `[???]` Tags\n"]
for driver in sorted(data.keys(), reverse=True):
    total_profiles = len(profile_counts[driver])
    sorted_issues = sorted(data[driver].items(), key=lambda x: x[1], reverse=True)
    markdown_output.append(
        f"#### Driver `{driver}` ({total_profiles} profiles, {len(sorted_issues)} issues)"
    )
    markdown_output.append("```text")

    if sorted_issues:
        for issue, count in sorted_issues:
            sorted_gpus = sorted(gpus[driver][issue])
            markdown_output.append(
                f"{issue} - {count} profiles ({', '.join(sorted_gpus)})"
            )
    else:
        markdown_output.append("No unknown [???] tags detected.")
    markdown_output.append("```\n")

final_markdown = "\n".join(markdown_output)

os.makedirs(base_dir, exist_ok=True)
with open(os.path.join(base_dir, "README.md"), "w", encoding="utf-8") as out:
    out.write(final_markdown)

summary_env = os.environ.get("GITHUB_STEP_SUMMARY")
if summary_env:
    with open(summary_env, "a", encoding="utf-8") as github_summary:
        github_summary.write(final_markdown)

print(final_text)
