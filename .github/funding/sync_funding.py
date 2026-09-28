import json
import os
import re
import sys
import urllib.request
from pathlib import Path

API_KEY = os.environ.get("LEMONSQUEEZY_API_KEY")
PRODUCT_ID = os.environ.get("LEMONSQUEEZY_PRODUCT_ID")

README_PATH = Path("README.md")
SVG_OUTPUT_PATH = Path(".github/funding/progress.svg")

if not API_KEY or not PRODUCT_ID:
    print("Error: LEMONSQUEEZY_API_KEY and LEMONSQUEEZY_PRODUCT_ID must be set.")
    sys.exit(1)


def fetch_product_revenue_usd(api_key: str, target_product_id: str) -> float:
    """Fetches all paid orders from Lemon Squeezy and sums net USD revenue for the product."""
    url = "https://api.lemonsqueezy.com/v1/orders?page[size]=100"
    headers = {
        "Accept": "application/vnd.api+json",
        "Content-Type": "application/vnd.api+json",
        "Authorization": f"Bearer {api_key}",
        "User-Agent": "GitHub-Funding-Sync/1.0",
    }

    total_cents = 0
    target_id_str = str(target_product_id).strip()

    while url:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=20) as response:
            payload = json.loads(response.read().decode("utf-8"))

        for order in payload.get("data", []):
            attrs = order.get("attributes", {})
            status = attrs.get("status")
            first_item = attrs.get("first_order_item") or {}
            prod_id = str(first_item.get("product_id", ""))

            if status == "paid" and prod_id == target_id_str:
                subtotal_cents = attrs.get("subtotal_usd", 0)
                refunded_cents = attrs.get("refunded_amount_usd", 0)
                net_cents = max(0, subtotal_cents - refunded_cents)
                total_cents += net_cents

        url = payload.get("links", {}).get("next")

    return total_cents / 100.0


def format_amount(val: float) -> str:
    """Formats whole dollars without decimals (50) and cents with 2 decimals (50.25)."""
    return f"{int(val)}" if val.is_integer() else f"{val:.2f}"


def build_svg(progress_pct: int, bar_width: int, is_completed: bool) -> str:
    """Generates the SVG progress bar and displays 'Goal Completed!' once reached."""
    if is_completed:
        label_text = f"Goal Completed! ({progress_pct}%)"
        aria_text = f"Goal Completed at {progress_pct} percent funding progress"
        bar_fill = "#16a34a"
    else:
        label_text = f"{progress_pct}%"
        aria_text = f"{progress_pct} percent funding progress"
        bar_fill = "#22c55e"

    return (
        f'<svg width="400" height="28" viewBox="0 0 400 28" xmlns="http://www.w3.org/2000/svg" '
        f'role="img" aria-label="{aria_text}">\n'
        f'  <defs>\n'
        f'    <clipPath id="pill-clip">\n'
        f'      <rect x="0" y="2" width="400" height="24" rx="12"/>\n'
        f'    </clipPath>\n'
        f'  </defs>\n'
        f'  <rect x="0" y="2" width="400" height="24" rx="12" fill="#2d3748"/>\n'
        f'  <rect x="0" y="2" width="{bar_width}" height="24" fill="{bar_fill}" clip-path="url(#pill-clip)"/>\n'
        f'  <text x="200" y="19" text-anchor="middle" font-family="Arial, sans-serif" '
        f'font-size="13" font-weight="bold" fill="#ffffff">{label_text}</text>\n'
        f'</svg>\n'
    )


def main():
    if not README_PATH.exists():
        print("Error: README.md not found.")
        sys.exit(1)

    content = README_PATH.read_text(encoding="utf-8")

    # 1. Isolate the funding section in README.md
    block_pattern = re.compile(
        r"(<!-- LEMON-SQUEEZY-START -->)(.*?)(<!-- LEMON-SQUEEZY-END -->)",
        re.DOTALL,
    )
    match = block_pattern.search(content)
    target_section = match.group(2) if match else content

    # 2. Extract the Goal amount from the Goal badge (e.g. Goal-$200)
    goal_match = re.search(r"badge/Goal-\$([0-9]+(?:\.[0-9]+)?)-", target_section)
    if not goal_match:
        print("Error: Could not find 'badge/Goal-$<number>-' in README.md")
        sys.exit(1)

    goal_usd = float(goal_match.group(1))

    # 3. Fetch paid product revenue from Lemon Squeezy
    raised_usd = fetch_product_revenue_usd(API_KEY, PRODUCT_ID)

    # 4. Calculate percentage, completion state, and SVG fill width (max 400px)
    raw_pct = (raised_usd / goal_usd * 100) if goal_usd > 0 else 0
    progress_pct = int(round(raw_pct))
    is_completed = raised_usd >= goal_usd and goal_usd > 0

    clamped_pct = min(100, max(0, progress_pct))
    bar_width = 400 if is_completed else int(round((clamped_pct / 100.0) * 400))

    raised_str = format_amount(raised_usd)
    goal_str = format_amount(goal_usd)

    # 5. Build Progress badge URL & alt text based on completion status
    if is_completed:
        progress_badge = f"badge/Progress-Goal_Completed!_({progress_pct}%25)-22c55e"
        progress_alt = f'alt="Funding Progress Goal Completed ({progress_pct}%)"'
    else:
        progress_badge = f"badge/Progress-{progress_pct}%25-ea580c"
        progress_alt = f'alt="Funding Progress {progress_pct}%"'

    # 6. Update badges and alt attributes in README.md
    updated_section = re.sub(
        r"badge/Raised-\$[0-9.,]+-22c55e",
        f"badge/Raised-${raised_str}-22c55e",
        target_section,
    )
    updated_section = re.sub(
        r'alt="Raised \$[0-9.,]+"',
        f'alt="Raised ${raised_str}"',
        updated_section,
    )
    updated_section = re.sub(
        r'alt="Funding Goal \$[0-9.,]+"',
        f'alt="Funding Goal ${goal_str}"',
        updated_section,
    )
    updated_section = re.sub(
        r"badge/Progress-[^?]+",
        progress_badge,
        updated_section,
    )
    updated_section = re.sub(
        r'alt="Funding Progress [^"]+"',
        progress_alt,
        updated_section,
    )

    # 7. Write the updated SVG to .github/workflow/funding/progress.svg
    SVG_OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    SVG_OUTPUT_PATH.write_text(
        build_svg(progress_pct, bar_width, is_completed), encoding="utf-8"
    )

    # 8. Save updated README.md
    if match:
        new_content = (
            content[: match.start(2)] + updated_section + content[match.end(2) :]
        )
    else:
        new_content = updated_section

    README_PATH.write_text(new_content, encoding="utf-8")
    status_msg = "GOAL COMPLETED!" if is_completed else "In Progress"
    print(f"Synced ({status_msg}): Raised ${raised_str} / ${goal_str} ({progress_pct}%)")


if __name__ == "__main__":
    main()
