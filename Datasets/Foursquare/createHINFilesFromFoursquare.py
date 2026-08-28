import csv
from pathlib import Path

import kagglehub
import pandas as pd


DATASET_HANDLE = "chetanism/foursquare-nyc-and-tokyo-checkin-dataset"
OUTPUT_DIR = Path(__file__).resolve().parent

USER_COLUMN = "userId"
VENUE_COLUMN = "venueId"
CATEGORY_COLUMN = "venueCategoryId"
LATITUDE_COLUMN = "latitude"
LONGITUDE_COLUMN = "longitude"

NODE_TYPES = {
    "user": 0,
    "venue": 1,
    "venue_category": 2,
}

EDGE_TYPES = {
    "checked_in_at": 0,
    "has_category": 1,
}

SCHEMA = {
    "checked_in_at": ("user", "venue"),
    "has_category": ("venue", "venue_category"),
}

entity_id_to_hin_id = {}
venue_to_seq_id = {}
counts_per_node_type = {}
counts_per_edge_type = {}


def ensure_dirs(output_dir):
    Path(output_dir, "mapping").mkdir(parents=True, exist_ok=True)


def find_dataset_files(dataset_path):
    dataset_path = Path(dataset_path)
    files = sorted(dataset_path.glob("dataset_TSMC2014_*.csv"))

    if not files:
        files = sorted(dataset_path.glob("*.csv"))

    if not files:
        raise FileNotFoundError(f"No CSV files found in {dataset_path}")

    return files


def load_checkins():
    dataset_path = kagglehub.dataset_download(DATASET_HANDLE)
    print(f"Path to dataset files: {dataset_path}")

    files = find_dataset_files(dataset_path)
    dataframes = []

    for file in files:
        print(f"Loading {file.name}")

        df = pd.read_csv(
            file,
            usecols=[
                USER_COLUMN,
                VENUE_COLUMN,
                CATEGORY_COLUMN,
                LATITUDE_COLUMN,
                LONGITUDE_COLUMN,
            ],
            dtype={
                USER_COLUMN: str,
                VENUE_COLUMN: str,
                CATEGORY_COLUMN: str,
            },
        )

        # User IDs are only unique within each city dataset.
        # Namespace them by the source file/city.
        filename_lower = file.stem.lower()

        if "nyc" in filename_lower:
            city = "NYC"
        elif "tky" in filename_lower or "tokyo" in filename_lower:
            city = "TKY"
        else:
            raise ValueError(f"Could not determine city from filename: {file.name}")

        df[USER_COLUMN] = city + ":" + df[USER_COLUMN]

        dataframes.append(df)

    checkins = pd.concat(dataframes, ignore_index=True)

    checkins[LATITUDE_COLUMN] = pd.to_numeric(checkins[LATITUDE_COLUMN], errors="coerce")
    checkins[LONGITUDE_COLUMN] = pd.to_numeric(checkins[LONGITUDE_COLUMN], errors="coerce")

    checkins = checkins[
        checkins[LATITUDE_COLUMN].between(-90, 90)
        & checkins[LONGITUDE_COLUMN].between(-180, 180)
    ]

    checkins = checkins.dropna(
        subset=[
            USER_COLUMN,
            VENUE_COLUMN,
            LATITUDE_COLUMN,
            LONGITUDE_COLUMN,
        ]
    )

    checkins = checkins.drop_duplicates(
        subset=[
            USER_COLUMN,
            VENUE_COLUMN,
            CATEGORY_COLUMN,
            LATITUDE_COLUMN,
            LONGITUDE_COLUMN,
        ]
    )

    print(f"Valid check-in rows: {len(checkins):,}")
    return checkins


def write_node_types(output_dir):
    with open(Path(output_dir, "node_types.csv"), "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        for node_type, _ in sorted(NODE_TYPES.items(), key=lambda item: item[1]):
            writer.writerow([node_type])


def write_edge_types(output_dir):
    with open(Path(output_dir, "edge_types.csv"), "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        for edge_type, _ in sorted(EDGE_TYPES.items(), key=lambda item: item[1]):
            writer.writerow([edge_type])


def write_schema(output_dir):
    with open(Path(output_dir, "schema.csv"), "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        for edge_type, _ in sorted(EDGE_TYPES.items(), key=lambda item: item[1]):
            source_node_type, target_node_type = SCHEMA[edge_type]
            writer.writerow([NODE_TYPES[source_node_type], NODE_TYPES[target_node_type]])


def extract_nodes(checkins, output_dir):
    write_node_types(output_dir)

    users = sorted(checkins[USER_COLUMN].unique())
    venues = sorted(checkins[VENUE_COLUMN].unique())
    venue_categories = sorted(checkins[CATEGORY_COLUMN].dropna().unique())

    current_hin_id = 0

    with open(Path(output_dir, "nodes.csv"), "w", newline="", encoding="utf-8") as nodes_file, \
            open(Path(output_dir, "mapping", "user_mapping.csv"), "w", newline="", encoding="utf-8") as user_mapping_file, \
            open(Path(output_dir, "mapping", "venue_mapping.csv"), "w", newline="", encoding="utf-8") as venue_mapping_file, \
            open(Path(output_dir, "mapping", "venue_category_mapping.csv"), "w", newline="", encoding="utf-8") as category_mapping_file:

        nodes_writer = csv.writer(nodes_file)

        user_writer = csv.writer(user_mapping_file)
        venue_writer = csv.writer(venue_mapping_file)
        category_writer = csv.writer(category_mapping_file)

        for sequential_id, user_id in enumerate(users):
            entity_id_to_hin_id[("user", user_id)] = current_hin_id
            nodes_writer.writerow([NODE_TYPES["user"]])
            user_writer.writerow([sequential_id, current_hin_id, user_id])
            current_hin_id += 1

        counts_per_node_type["user"] = len(users)

        for sequential_id, venue_id in enumerate(venues):
            entity_id_to_hin_id[("venue", venue_id)] = current_hin_id
            venue_to_seq_id[venue_id] = sequential_id # Used to map venues (from 0 to #venues-1) to coordinates
            nodes_writer.writerow([NODE_TYPES["venue"]])
            venue_writer.writerow([sequential_id, current_hin_id, venue_id])
            current_hin_id += 1

        counts_per_node_type["venue"] = len(venues)

        for sequential_id, category_id in enumerate(venue_categories):
            entity_id_to_hin_id[("venue_category", category_id)] = current_hin_id
            nodes_writer.writerow([NODE_TYPES["venue_category"]])
            category_writer.writerow([sequential_id, current_hin_id, category_id])
            current_hin_id += 1

        counts_per_node_type["venue_category"] = len(venue_categories)

    print(f"Total HIN nodes created: {current_hin_id:,}")


def extract_edges(checkins, output_dir):
    write_edge_types(output_dir)
    write_schema(output_dir)

    edges = set()
    seen = set()

    for _, row in checkins.iterrows():
        user_id = row[USER_COLUMN]
        venue_id = row[VENUE_COLUMN]
        category_id = row[CATEGORY_COLUMN]

        user_hin_id = entity_id_to_hin_id[("user", user_id)]
        venue_hin_id = entity_id_to_hin_id[("venue", venue_id)]

        if (user_hin_id, EDGE_TYPES["checked_in_at"], venue_hin_id) not in seen:
            edges.add((user_hin_id, EDGE_TYPES["checked_in_at"], venue_hin_id))
            seen.add((user_hin_id, EDGE_TYPES["checked_in_at"], venue_hin_id))

        if pd.notna(category_id):
            category_hin_id = entity_id_to_hin_id[("venue_category", category_id)]

            if (venue_hin_id, EDGE_TYPES["has_category"], category_hin_id) not in seen:
                edges.add((venue_hin_id, EDGE_TYPES["has_category"], category_hin_id))
                seen.add((venue_hin_id, EDGE_TYPES["has_category"], category_hin_id))

    edges = sorted(edges, key=lambda edge: (edge[0], edge[1], edge[2]))

    with open(Path(output_dir, "edges.csv"), "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        for source_id, edge_type_id, target_id in edges:
            writer.writerow([source_id, edge_type_id, target_id])

    for edge_type_name, edge_type_id in EDGE_TYPES.items():
        counts_per_edge_type[edge_type_name] = sum(
            1 for _, current_edge_type_id, _ in edges
            if current_edge_type_id == edge_type_id
        )

    print(f"Total HIN edges created: {len(edges):,}")


def store_coordinates(checkins, output_dir):
    with open(Path(output_dir, "venuesWGS84.csv"), "w", newline="", encoding="utf-8") as file:
        seen = set()
        triplets = []

        for _, row in checkins.iterrows():
            seq_id = venue_to_seq_id[row[VENUE_COLUMN]]
            if seq_id not in seen:
                triplets.append((seq_id, row[LONGITUDE_COLUMN], row[LATITUDE_COLUMN]))
                seen.add(seq_id)

        triplets.sort(key=lambda t: t[0])

        writer = csv.writer(file)
        for triplet in triplets:
            writer.writerow(triplet)

    print(f"Stored coordinates for {len(seen):,} venues.")


def check_duplicate_edges(output_dir):
    seen = set()
    duplicate_count = 0

    with open(Path(output_dir, "edges.csv"), "r", encoding="utf-8") as file:
        reader = csv.reader(file)
        for row in reader:
            edge = tuple(row)
            if edge in seen:
                duplicate_count += 1
            else:
                seen.add(edge)

    if duplicate_count == 0:
        print("No duplicate edges found.")
    else:
        print(f"Found {duplicate_count:,} duplicate edges.")


def print_counts():
    print("\n===== NODE COUNTS =====")
    for node_type, count in counts_per_node_type.items():
        print(f"{node_type}: {count:,} instances")

    print("\n===== EDGE COUNTS =====")
    for edge_type, count in counts_per_edge_type.items():
        print(f"{edge_type}: {count:,} instances")


if __name__ == "__main__":
    ensure_dirs(OUTPUT_DIR)

    checkins_df = load_checkins()

    extract_nodes(checkins_df, OUTPUT_DIR)
    extract_edges(checkins_df, OUTPUT_DIR)
    store_coordinates(checkins_df, OUTPUT_DIR)
    check_duplicate_edges(OUTPUT_DIR)
    print_counts()
