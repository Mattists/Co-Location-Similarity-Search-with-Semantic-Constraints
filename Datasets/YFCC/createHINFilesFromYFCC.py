from huggingface_hub import list_repo_files, hf_hub_download
import pandas as pd
import csv
import os
from pathlib import Path

REPO_ID = "dalle-mini/YFCC100M_OpenAI_subset"
REPO_TYPE = "dataset"

MAX_SHARDS = 1000
OUTPUT_DIR = Path(__file__).resolve().parent

entity_id_to_hin_id = {}
photo_to_seq_id = {}
counts_per_node_type = {}
counts_per_edge_type = {}

NODE_TYPES = {
    "user": 0,
    "photo": 1
}

EDGE_TYPES = {
    "posted": 0
}

SCHEMA = {
    "posted": ("user", "photo")
}


def ensure_dirs(output_dir="."):
    os.makedirs(f"{output_dir}/mapping", exist_ok=True)


def load_yfcc_metadata(max_shards=1000):
    relevant_columns = ["photoid", "uid", "longitude", "latitude"]

    files = list_repo_files(REPO_ID, repo_type=REPO_TYPE)
    metadata_files = sorted([
        f for f in files
        if f.startswith("metadata/") and f.endswith(".jsonl.gz")
    ])

    print(f"Available metadata shards: {len(metadata_files):,}")
    print(f"Loading metadata shards: {min(max_shards, len(metadata_files)):,}")

    dfs = []

    for metadata_file in metadata_files[:max_shards]:

        local_path = hf_hub_download(
            repo_id=REPO_ID,
            repo_type=REPO_TYPE,
            filename=metadata_file
        )

        chunk = pd.read_json(
            local_path,
            orient="records",
            lines=True,
            compression="gzip"
        )

        chunk = chunk[relevant_columns]
        dfs.append(chunk)

    df = pd.concat(dfs, ignore_index=True)

    df["longitude"] = pd.to_numeric(df["longitude"], errors="coerce")
    df["latitude"] = pd.to_numeric(df["latitude"], errors="coerce")

    df = df.dropna(subset=["photoid", "uid", "longitude", "latitude"])

    df["photoid"] = df["photoid"].astype(str)
    df["uid"] = df["uid"].astype(str)

    df = df.drop_duplicates(subset=["photoid", "uid", "longitude", "latitude"])

    print(f"Rows with valid coordinates: {len(df):,}")

    return df


def write_schema(output_dir="."):
    with open(f"{output_dir}/schema.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for edge_type_name, edge_type_id in sorted(EDGE_TYPES.items(), key=lambda item: item[1]):
            source_node_type, target_node_type = SCHEMA[edge_type_name]
            writer.writerow([NODE_TYPES[source_node_type], NODE_TYPES[target_node_type]])


def extract_nodes(df, output_dir="."):
    with open(f"{output_dir}/node_types.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for node_type_name, node_type_id in sorted(NODE_TYPES.items(), key=lambda item: item[1]):
            writer.writerow([node_type_name])

    users = sorted(df["uid"].unique())
    photos = sorted(df["photoid"].unique())

    current_hin_node_id = 0

    with open(f"{output_dir}/nodes.csv", "w", newline="", encoding="utf-8") as nodes_file, \
         open(f"{output_dir}/mapping/user_mapping.csv", "w", newline="", encoding="utf-8") as user_mapping, \
         open(f"{output_dir}/mapping/photo_mapping.csv", "w", newline="", encoding="utf-8") as photo_mapping:

        nodes_writer = csv.writer(nodes_file)

        user_writer = csv.writer(user_mapping)
        photo_writer = csv.writer(photo_mapping)

        print("Extracting users...")
        for sequential_index, user_id in enumerate(users):
            entity_id_to_hin_id[("user", user_id)] = current_hin_node_id
            nodes_writer.writerow([NODE_TYPES["user"]])
            user_writer.writerow([sequential_index, current_hin_node_id, user_id])
            current_hin_node_id += 1

        counts_per_node_type["user"] = len(users)

        print("Extracting photos...")
        for sequential_index, photo_id in enumerate(photos):
            entity_id_to_hin_id[("photo", photo_id)] = current_hin_node_id
            photo_to_seq_id[photo_id] = sequential_index # Used to map photos (from 0 to #photos-1) to coordinates
            nodes_writer.writerow([NODE_TYPES["photo"]])
            photo_writer.writerow([sequential_index, current_hin_node_id, photo_id])
            current_hin_node_id += 1

        counts_per_node_type["photo"] = len(photos)

    print(f"Total HIN nodes created: {current_hin_node_id}")


def extract_edges(df, output_dir="."):
    with open(f"{output_dir}/edge_types.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for edge_type_name, edge_type_id in sorted(EDGE_TYPES.items(), key=lambda item: item[1]):
            writer.writerow([edge_type_name])
    write_schema(output_dir)

    edges_data = []
    seen = set()

    print("Extracting edges...")

    for _, row in df.iterrows():
        user_id = row["uid"]
        photo_id = row["photoid"]

        user_hin_id = entity_id_to_hin_id.get(("user", user_id))
        photo_hin_id = entity_id_to_hin_id.get(("photo", photo_id))

        if user_hin_id is not None and photo_hin_id is not None:
            edge = (user_hin_id, EDGE_TYPES["posted"], photo_hin_id)
            if edge not in seen:
                edges_data.append(edge)
                seen.add(edge)

    edges_data.sort(key=lambda x: (x[0], x[1], x[2]))

    with open(f"{output_dir}/edges.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for src, edge_type, dst in edges_data:
            writer.writerow([src, edge_type, dst])

    counts_per_edge_type["posted"] = sum(1 for _, edge_type, _ in edges_data if edge_type == EDGE_TYPES["posted"])

    print(f"Total edges created: {len(edges_data):,}")


def store_coordinates(df, output_dir="."):

    with open(f"{output_dir}/photosWGS84.csv", "w", newline="", encoding="utf-8") as outfile:

        seen = set()
        triplets = []

        for _, row in df.iterrows():
            seq_id = photo_to_seq_id[row['photoid']]
            if seq_id not in seen:
                triplets.append((seq_id, row["longitude"], row["latitude"]))
                seen.add(seq_id)

        triplets.sort(key=lambda t: t[0])

        writer = csv.writer(outfile)
        for triplet in triplets:
            writer.writerow(triplet)

    print(f"Stored coordinates for {len(seen):,} photos.")


def check_duplicate_edges(output_dir="."):
    seen = set()
    duplicates = []

    with open(f"{output_dir}/edges.csv", "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            edge = tuple(row)
            if edge in seen:
                duplicates.append(edge)
            else:
                seen.add(edge)

    if duplicates:
        print(f"Found {len(duplicates):,} duplicate edges!")
        print("Sample duplicates:", duplicates[:10])
    else:
        print("No duplicate edges found.")


if __name__ == "__main__":
    ensure_dirs(OUTPUT_DIR)

    df = load_yfcc_metadata(MAX_SHARDS)

    extract_nodes(df, OUTPUT_DIR)
    extract_edges(df, OUTPUT_DIR)
    store_coordinates(df, OUTPUT_DIR)

    print("\n===== NODE COUNTS =====")
    for key, value in counts_per_node_type.items():
        print(f"{key}: {value:,} instances")

    print("\n===== EDGE COUNTS =====")
    for key, value in counts_per_edge_type.items():
        print(f"{key}: {value:,} instances")

    check_duplicate_edges(OUTPUT_DIR)
