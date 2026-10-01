#!/usr/bin/env python3
"""
KryonOS Help Center Index & URL Generator
Automatically scans help/categories/, generates category.json for each folder,
and creates the root help/index.json with raw GitHub links.
"""

import os
import json
import re
import sys
from pathlib import Path

DEFAULT_REPO = os.environ.get("GITHUB_REPOSITORY", "Haris16-code/KryonOS")
DEFAULT_BRANCH = os.environ.get("GITHUB_REF_NAME", "main")

def format_category_name(folder_name: str) -> str:
    # Remove leading number prefix if present, e.g. "01-getting-started" -> "Getting Started"
    clean = re.sub(r'^\d+[-_]', '', folder_name)
    clean = clean.replace('-', ' ').replace('_', ' ')
    words = clean.split()
    capitalized = []
    for w in words:
        if w.lower() in ['and', 'to', 'of', 'for', 'in', 'vs']:
            capitalized.append(w.lower() if len(capitalized) > 0 else w.capitalize())
        elif w.lower() == 'wifi':
            capitalized.append('Wi-Fi')
        elif w.lower() == 'ota':
            capitalized.append('OTA')
        elif w.lower() == 'faqs':
            capitalized.append('FAQs')
        elif w.lower() == 'ai':
            capitalized.append('AI')
        elif w.lower() == 'kryonos':
            capitalized.append('KryonOS')
        elif w.lower() == 'kryoncloud':
            capitalized.append('KryonCloud')
        elif w.lower() == 'kryonbeam':
            capitalized.append('KryonBeam')
        else:
            capitalized.append(w.capitalize())
    return ' '.join(capitalized)

def generate(repo: str = DEFAULT_REPO, branch: str = DEFAULT_BRANCH, root_dir: str = None):
    if root_dir is None:
        script_path = Path(__file__).resolve()
        # Look for workspace root (2 levels up from .github/scripts)
        workspace_root = script_path.parent.parent.parent
    else:
        workspace_root = Path(root_dir).resolve()

    help_dir = workspace_root / "help"
    categories_dir = help_dir / "categories"

    if not categories_dir.exists():
        print(f"Error: {categories_dir} does not exist.")
        sys.exit(1)

    base_raw_url = f"https://raw.githubusercontent.com/{repo}/refs/heads/{branch}/help"
    
    cat_dirs = sorted([d for d in categories_dir.iterdir() if d.is_dir()])
    index_categories = []

    for cat_dir in cat_dirs:
        folder_name = cat_dir.name
        cat_display_name = format_category_name(folder_name)
        
        # Find all article json files (exclude category.json, index.json)
        art_files = sorted([
            f for f in cat_dir.glob("*.json")
            if f.name not in ["category.json", "index.json"]
        ])

        if not art_files:
            continue

        articles_list = []
        for art_file in art_files:
            try:
                with open(art_file, 'r', encoding='utf-8') as af:
                    art_data = json.load(af)
                title = art_data.get("title", art_file.stem.replace('-', ' ').replace('_', ' ').title())
            except Exception as e:
                print(f"Warning: Could not read title from {art_file}: {e}")
                title = art_file.stem.replace('-', ' ').replace('_', ' ').title()

            art_url = f"{base_raw_url}/categories/{folder_name}/{art_file.name}"
            articles_list.append({
                "title": title,
                "url": art_url
            })

        category_json_data = {
            "category": cat_display_name,
            "articles": articles_list
        }

        category_json_path = cat_dir / "category.json"
        with open(category_json_path, 'w', encoding='utf-8') as cjf:
            json.dump(category_json_data, cjf, indent=2, ensure_ascii=False)
            cjf.write('\n')
        print(f"Generated {category_json_path.relative_to(workspace_root)} ({len(articles_list)} articles)")

        category_raw_url = f"{base_raw_url}/categories/{folder_name}/category.json"
        index_categories.append({
            "name": cat_display_name,
            "url": category_raw_url
        })

    root_index_data = {
        "categories": index_categories
    }

    root_index_path = help_dir / "index.json"
    with open(root_index_path, 'w', encoding='utf-8') as rif:
        json.dump(root_index_data, rif, indent=2, ensure_ascii=False)
        rif.write('\n')
    print(f"Generated {root_index_path.relative_to(workspace_root)} ({len(index_categories)} categories)")

if __name__ == "__main__":
    repo_arg = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_REPO
    branch_arg = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_BRANCH
    print(f"Building Help Center indices for {repo_arg} @ {branch_arg}...")
    generate(repo_arg, branch_arg)
