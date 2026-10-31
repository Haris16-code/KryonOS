import json
import os
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

ACCESS_TOKEN = os.environ.get("PATREON_ACCESS_TOKEN")
CAMPAIGN_ID = os.environ.get("PATREON_CAMPAIGN_ID")

README_PATH = Path("README.md")
SVG_OUTPUT_PATH = Path(".github/funding/progress.svg")

if not ACCESS_TOKEN:
    print("Error: PATREON_ACCESS_TOKEN must be set.")
    sys.exit(1)


def fetch_patreon_pledge_revenue_usd(access_token: str, target_campaign_id: str | None = None) -> float:
    """Fetches active monthly pledge total using Patreon API v2 members endpoint."""
    headers = {
        "Authorization": f"Bearer {access_token.strip()}",
        "User-Agent": "GitHub-Funding-Sync/1.0",
    }

    campaign_id = target_campaign_id.strip() if target_campaign_id else None

    # 1. Resolve Campaign ID if not explicitly specified
    if not campaign_id:
        query = urllib.parse.urlencode({"fields[campaign]": "creation_name,patron_count"})
        url = f"https://www.patreon.com/api/oauth2/v2/campaigns?{query}"
        req = urllib.request.Request(url, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as err:
            print(f"Patreon API HTTP error {err.code}: {err.read().decode('utf-8', errors='ignore')}")
            sys.exit(1)

        campaigns = payload.get("data", [])
        if not campaigns:
            print("Error: No Patreon campaigns found for this account.")
            sys.exit(1)
        campaign_id = str(campaigns[0].get("id"))

    # 2. Page through campaign members and sum monthly pledge amounts from active patrons
    members_query = urllib.parse.urlencode({
        "page[size]": "100",
        "fields[member]": "currently_entitled_amount_cents,patron_status",
    })
    members_url = f"https://www.patreon.com/api/oauth2/v2/campaigns/{campaign_id}/members?{members_query}"

    total_cents = 0

    while members_url:
        req = urllib.request.Request(members_url, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as err:
            err_body = err.read().decode("utf-8", errors="ignore")
            print(f"Patreon API HTTP error {err.code}: {err_body}")
            if err.code == 403:
                print(
                    "\nTip: 403 Forbidden indicates your token is missing the 'campaigns.members' scope.\n"
                    "In your Patreon Developer Portal client settings, ensure 'campaigns.members' is enabled."
                )
            sys.exit(1)

        for member in payload.get("data", []):
            attrs = member.get("attributes", {})
            status = attrs.get("patron_status")
            if status == "active_patron":
                # currently_entitled_amount_cents reflects their active recurring pledge
                total_cents += attrs.get("currently_entitled_amount_cents", 0)

        members_url = payload.get("links", {}).get("next")

    return total_cents / 100.0


def format_amount(val: float) -> str:
    """Formats whole dollars without decimals (50) and cents with 2 decimals (50.25)."""
    return f"{int(val)}" if val.is_integer() else f"{val:.2f}"


def build_svg(progress_pct: int, bar_width: int, is_completed: bool) -> str:
    """Generates the SVG progress bar."""
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
        r"(<!-- (?:PATREON|LEMON-SQUEEZY)-START -->)(.*?)(<!-- (?:PATREON|LEMON-SQUEEZY)-END -->)",
        re.DOTALL,
    )
    match = block_pattern.search(content)
    target_section = match.group(2) if match else content

    # 2. Extract Goal from badge
    goal_match = re.search(r"badge/Goal-\$([0-9]+(?:\.[0-9]+)?)-", target_section)
    if not goal_match:
        print("Error: Could not find 'badge/Goal-$<number>-' in README.md")
        sys.exit(1)

    goal_usd = float(goal_match.group(1))

    # 3. Fetch active pledge sum
    raised_usd = fetch_patreon_pledge_revenue_usd(ACCESS_TOKEN, CAMPAIGN_ID)

    # 4. Calculate progress
    raw_pct = (raised_usd / goal_usd * 100) if goal_usd > 0 else 0
    progress_pct = int(round(raw_pct))
    is_completed = raised_usd >= goal_usd and goal_usd > 0

    clamped_pct = min(100, max(0, progress_pct))
    bar_width = 400 if is_completed else int(round((clamped_pct / 100.0) * 400))

    raised_str = format_amount(raised_usd)
    goal_str = format_amount(goal_usd)

    # 5. Build badges
    if is_completed:
        progress_badge = f"badge/Progress-Goal_Completed!_({progress_pct}%25)-22c55e"
        progress_alt = f'alt="Funding Progress Goal Completed ({progress_pct}%)"'
    else:
        progress_badge = f"badge/Progress-{progress_pct}%25-ea580c"
        progress_alt = f'alt="Funding Progress {progress_pct}%"'

    # 6. Replace in README
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

    # 7. Write SVG
    SVG_OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    SVG_OUTPUT_PATH.write_text(
        build_svg(progress_pct, bar_width, is_completed), encoding="utf-8"
    )

    # 8. Save updated README
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
