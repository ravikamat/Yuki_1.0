#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
yuki_skill_miner.py — Background Skill Pattern Miner
Yuki_1.0 — §3.8 ExperienceRefinery

Runs every 5 minutes as a background thread inside the knowledge daemon
OR standalone via: python yuki_skill_miner.py

What it does:
  1. Reads data/traces/yuki_traces.jsonl (append-only trace log)
  2. Groups traces by coreIntent (similar topic/task)
  3. When the same task type appears 2+ times → crystallizes a RuntimeSkill
  4. Writes skill JSON to data/skills/mined_<id>.json
  5. Next startup: SkillRegistry hot-loads it → fires immediately

This is how Yuki learns to handle things faster over time with ZERO user input.
"""

import sys, os, json, time, re, hashlib
from collections import defaultdict
from pathlib import Path

TRACES_FILE  = "data/traces/yuki_traces.jsonl"
SKILLS_DIR   = "data/skills/"
MIN_HITS     = 2       # need to see a pattern this many times before crystallizing
CHECK_EVERY  = 300     # seconds between checks (5 minutes)

os.makedirs(SKILLS_DIR, exist_ok=True)

# ── Already-mined skill ids (avoid duplicates) ────────────────────────────────

def load_existing_skill_ids():
    ids = set()
    for f in Path(SKILLS_DIR).glob("*.json"):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
            ids.add(data.get("id",""))
        except Exception:
            pass
    return ids

# ── Read traces ───────────────────────────────────────────────────────────────

def read_traces():
    if not os.path.exists(TRACES_FILE):
        return []
    traces = []
    with open(TRACES_FILE, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try:
                traces.append(json.loads(line))
            except Exception:
                pass
    return traces

# ── Intent normalization — strip stop words to find topic ────────────────────

STOPWORDS = {"what","is","the","a","an","how","do","does","can","will","yuki",
             "please","me","my","i","you","to","in","on","at","for","of","and"}

def normalize_intent(text):
    words = re.findall(r"\b[a-z]{3,}\b", text.lower())
    return " ".join(w for w in words if w not in STOPWORDS)[:60]

# ── Group traces by normalized intent ─────────────────────────────────────────

def group_by_pattern(traces):
    groups = defaultdict(list)
    for t in traces:
        raw = t.get("input", {}).get("rawText", "")
        if not raw: continue
        key = normalize_intent(raw)
        if key:
            groups[key].append(t)
    return groups

# ── Detect task category from text ────────────────────────────────────────────

CATEGORY_PATTERNS = {
    "GOOGLE_SEARCH":  ["search for","google","look up","find information about"],
    "WEATHER_CHECK":  ["weather","temperature","forecast","will it rain"],
    "EMAIL_SEND":     ["send email","email to","send mail","write email"],
    "REMINDER_SET":   ["remind me","set reminder","set alarm","notify me"],
    "SCREENSHOT":     ["screenshot","screen capture","capture screen"],
    "SYSTEM_INFO":    ["cpu usage","ram usage","memory usage","disk space","system info"],
    "TRANSLATE":      ["translate","in spanish","in hindi","in french","in german"],
    "FILE_FIND":      ["find file","where is","locate file","search file"],
    "CALCULATOR":     ["calculate","compute","how much is","what is"],
    "WEB_OPEN":       ["open ","go to ","navigate to",".com",".org"],
}

def detect_category(texts):
    for text in texts:
        L = text.lower()
        for cat, pats in CATEGORY_PATTERNS.items():
            for p in pats:
                if p in L:
                    return cat
    return None

# ── Skill templates per category ──────────────────────────────────────────────

SKILL_TEMPLATES = {
    "GOOGLE_SEARCH": {
        "name": "Mined: Google Search",
        "description": "Search Google and return results",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/search_google.py",
        "triggerPatterns": ["search for ","google ","look up ","find information about "]
    },
    "WEATHER_CHECK": {
        "name": "Mined: Weather Check",
        "description": "Check weather for any city",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/weather_check.py",
        "triggerPatterns": ["weather in ","temperature in ","forecast for ","will it rain"]
    },
    "EMAIL_SEND": {
        "name": "Mined: Send Email",
        "description": "Compose and send an email",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/send_email.py",
        "triggerPatterns": ["send email to ","email to ","send mail to ","write email to "]
    },
    "REMINDER_SET": {
        "name": "Mined: Set Reminder",
        "description": "Set a timed reminder",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/set_reminder.py",
        "triggerPatterns": ["remind me ","set reminder ","set alarm ","notify me at "]
    },
    "SCREENSHOT": {
        "name": "Mined: Screenshot",
        "description": "Take a screenshot",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/screenshot.py",
        "triggerPatterns": ["screenshot","take screenshot","screen capture"]
    },
    "SYSTEM_INFO": {
        "name": "Mined: System Info",
        "description": "Get CPU/RAM/disk info",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/system_info.py",
        "triggerPatterns": ["cpu usage","ram usage","memory usage","disk space","system info"]
    },
    "TRANSLATE": {
        "name": "Mined: Translate",
        "description": "Translate text between languages",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/translate.py",
        "triggerPatterns": ["translate ","translate to ","in spanish","in hindi","in french"]
    },
    "FILE_FIND": {
        "name": "Mined: Find File",
        "description": "Search for files",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/find_file.py",
        "triggerPatterns": ["find file ","where is ","locate file "]
    },
    "CALCULATOR": {
        "name": "Mined: Calculator",
        "description": "Calculate expressions",
        "actionType": "RUN_SCRIPT",
        "actionTemplate": "SCRIPT:data/scripts/calculate.py",
        "triggerPatterns": ["calculate ","compute ","how much is "]
    },
}

# ── Mine traces → produce skills ─────────────────────────────────────────────

def mine(existing_ids):
    traces   = read_traces()
    if not traces:
        return []
    groups   = group_by_pattern(traces)
    new_skills = []

    for key, group in groups.items():
        if len(group) < MIN_HITS:
            continue

        # Get all raw texts from this group
        texts = [t.get("input",{}).get("rawText","") for t in group]
        cat   = detect_category(texts)
        if not cat or cat not in SKILL_TEMPLATES:
            continue

        # Make stable id from category
        skill_id = "mined_" + hashlib.md5(cat.encode()).hexdigest()[:10]
        if skill_id in existing_ids:
            continue

        tmpl = SKILL_TEMPLATES[cat]
        skill = {
            "id":             skill_id,
            "name":           tmpl["name"],
            "description":    tmpl["description"],
            "createdFrom":    f"[auto-mined] pattern={key!r} hits={len(group)}",
            "actionType":     tmpl["actionType"],
            "actionTemplate": tmpl["actionTemplate"],
            "timesUsed":      len(group),
            "triggerPatterns": tmpl["triggerPatterns"]
        }

        path = os.path.join(SKILLS_DIR, skill_id + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(skill, f, indent=2)

        new_skills.append(skill["name"])
        existing_ids.add(skill_id)
        print(f"[SkillMiner] Crystallized: {skill['name']} ({len(group)} hits) → {path}",
              flush=True)

    return new_skills

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    print("[SkillMiner] Background skill miner started", flush=True)
    existing_ids = load_existing_skill_ids()

    while True:
        try:
            new = mine(existing_ids)
            if new:
                print(f"[SkillMiner] Built {len(new)} new skills: {new}", flush=True)
        except Exception as e:
            print(f"[SkillMiner] Error: {e}", flush=True)
        time.sleep(CHECK_EVERY)

if __name__ == "__main__":
    main()
