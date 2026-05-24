#!/usr/bin/env python3
"""Post-process cs/pl/et/lv locales: fix untranslated keys and MT errors."""
from __future__ import annotations

import json
from pathlib import Path

LOC = Path(__file__).resolve().parents[1] / "loc"

# Keys still identical to en.json after auto-translate (protected terms + topmost)
FIXES: dict[str, dict[str, str]] = {
    "cs": {
        "base.catname.expl": "Průzkumník a pracovní plocha",
        "base.catname.compon": "Komponenty Windows",
        "base.catname.act": "Aktivace Windows",
        "base.def.on": "Zap.",
        "base.def.off": "Vyp.",
        "base.def.log_file": "Protokol",
        "expl.main.label": "Průzkumník a pracovní plocha",
        "compon.main.label": "Komponenty Windows",
        "act.main.label": "Aktivace Windows",
        "uwp.main.u20": "Poznámky",
        "sat.main.oned": "1 d",
        "pci.main.processorlabel": "Procesor",
        "pci.main.ramlabel": "Operační paměť",
        "pci.main.vlabel": "Grafická karta",
        "pci.main.benchtitle": "Benchmark MakuTweaker++",
        "pci.main.modeln": "Model",
        "pci.main.step_os": "Windows…",
        "pci.main.winsection": "Windows",
        "pci.main.vbslabel": "Zabezpečení založené na virtualizaci (VBS)",
        "pci.main.hypervlabel": "Hyper-V",
        "pci.main.defenderlabel": "Windows Defender",
        "pci.main.uaclabel": "Řízení uživatelských účtů (UAC)",
        "pci.main.smartscreenlabel": "SmartScreen",
        "pci.main.tpmlabel": "TPM",
        "pci.main.securebootlabel": "Secure Boot",
        "pci.main.st_enforced": "Zapnuto (vynuceno)",
        "pci.main.st_defender_rt": "Zapnuto (v reálném čase)",
        "per.main.c1": "Standardní",
        "per.main.b2": "Standardní",
        "ab.main.topmost": "Vždy nahoře",
        "mon.cpu.heatmap": "Teplotní mapa",
        "mon.cpu.topn_count": "Top N",
        "tools.main.automation": "Automatizace",
        "tools.main.dns_bench": "Test DNS",
        "tools.main.rollback": "Vrácení zpět",
        "mon.main.1h": "1 h",
        "mon.main.7d": "7 d",
        "mon.main.30d": "30 d",
    },
    "pl": {
        "base.catname.expl": "Eksplorator i Pulpit",
        "base.catname.compon": "Składniki systemu Windows",
        "base.catname.act": "Aktywacja Windows",
        "base.def.on": "Wł.",
        "base.def.off": "Wył.",
        "expl.main.label": "Eksplorator i Pulpit",
        "compon.main.label": "Składniki systemu Windows",
        "act.main.label": "Aktywacja Windows",
        "sat.main.os": "0 s",
        "sat.main.oned": "1 d",
        "per.main.c1": "Standardowy",
        "per.main.b2": "Standardowy",
        "pci.main.processorlabel": "Procesor",
        "pci.main.ramlabel": "Pamięć RAM",
        "pci.main.vlabel": "Karta graficzna",
        "pci.main.benchtitle": "Test MakuTweaker++",
        "pci.main.modeln": "Model",
        "pci.main.step_os": "Windows…",
        "pci.main.winsection": "Windows",
        "pci.main.regionlabel": "Region",
        "pci.main.vbslabel": "Zabezpieczenia oparte na wirtualizacji (VBS)",
        "pci.main.hypervlabel": "Hyper-V",
        "pci.main.defenderlabel": "Windows Defender",
        "pci.main.uaclabel": "Kontrola konta użytkownika (UAC)",
        "pci.main.smartscreenlabel": "SmartScreen",
        "pci.main.tpmlabel": "TPM",
        "pci.main.securebootlabel": "Secure Boot",
        "pci.main.st_enforced": "Włączone (wymuszone)",
        "pci.main.st_defender_rt": "Włączone (w czasie rzeczywistym)",
        "ab.main.topmost": "Zawsze na wierzchu",
        "mon.cpu.heatmap": "Mapa ciepła",
        "mon.cpu.topn_count": "Top N",
        "base.def.rebnotifyexplorer": "Zastosowana funkcja wymaga ponownego uruchomienia Eksploratora.",
        "tools.main.automation": "Automatyzacja",
        "tools.main.dns_bench": "Test DNS",
        "tools.main.rollback": "Przywracanie",
        "pci.main.batterystatus": "Stan",
        "pmgr.main.status": "Stan",
        "mon.main.7d": "7 d",
        "mon.main.30d": "30 d",
    },
    "et": {
        "base.catname.compon": "Windowsi komponendid",
        "base.catname.act": "Windowsi aktiveerimine",
        "compon.main.label": "Windowsi komponendid",
        "act.main.label": "Windowsi aktiveerimine",
        "sat.main.os": "0 s",
        "sat.main.oned": "1 p",
        "pci.main.processorlabel": "Protsessor",
        "pci.main.ramlabel": "RAM",
        "pci.main.vlabel": "Graafikakaart",
        "pci.main.benchtitle": "MakuTweaker++ test",
        "pci.main.step_os": "Windows…",
        "pci.main.winsection": "Windows",
        "pci.main.vbslabel": "Virtualiseerimispõhine turvalisus (VBS)",
        "pci.main.hypervlabel": "Hyper-V",
        "pci.main.defenderlabel": "Windows Defender",
        "pci.main.uaclabel": "Kasutajakonto juhtimine (UAC)",
        "pci.main.smartscreenlabel": "SmartScreen",
        "pci.main.tpmlabel": "TPM",
        "pci.main.securebootlabel": "Secure Boot",
        "pci.main.st_enforced": "Lubatud (sunni)",
        "pci.main.st_defender_rt": "Lubatud (reaalajas)",
        "ab.main.topmost": "Alati peal",
        "mon.cpu.heatmap": "Soojuskaart",
        "mon.cpu.topn_count": "Top N",
        "base.def.log_file": "Logi",
        "tools.main.automation": "Automatiseerimine",
        "tools.main.dns_bench": "DNS test",
        "tools.main.rollback": "Tagasipööramine",
        "pci.main.ramlabel": "RAM",
        "pci.main.modeln": "Mudel",
        "ab.main.import_mktw": "Impordi .mktw",
        "mon.main.1h": "1 t",
        "mon.main.6h": "6 t",
        "mon.main.24h": "24 t",
        "mon.main.7d": "7 p",
        "mon.main.30d": "30 p",
    },
    "lv": {
        "base.catname.compon": "Windows komponenti",
        "base.catname.act": "Windows aktivizācija",
        "base.catname.perf": "Veiktspēja",
        "compon.main.label": "Windows komponenti",
        "act.main.label": "Windows aktivizācija",
        "perfor.main.label": "Veiktspēja",
        "sat.main.os": "0 s",
        "sat.main.oned": "1 d",
        "pci.main.processorlabel": "Procesors",
        "pci.main.ramlabel": "RAM",
        "pci.main.vlabel": "Grafikas karte",
        "pci.main.benchtitle": "MakuTweaker++ tests",
        "pci.main.step_os": "Windows…",
        "pci.main.winsection": "Windows",
        "pci.main.vbslabel": "Uz virtualizāciju balstīta drošība (VBS)",
        "pci.main.hypervlabel": "Hyper-V",
        "pci.main.defenderlabel": "Windows Defender",
        "pci.main.uaclabel": "Lietotāja kontu kontrole (UAC)",
        "pci.main.smartscreenlabel": "SmartScreen",
        "pci.main.tpmlabel": "TPM",
        "pci.main.securebootlabel": "Secure Boot",
        "pci.main.st_enforced": "Ieslēgts (piespiedu)",
        "pci.main.st_defender_rt": "Ieslēgts (reāllaikā)",
        "ab.main.topmost": "Vienmēr virsū",
        "mon.cpu.heatmap": "Siltuma karte",
        "mon.cpu.topn_count": "Top N",
        "base.def.save_apply_failed": "Neizdevās lietot — pārbaudiet žurnālu vai atlasītās opcijas",
        "tools.main.automation": "Automatizācija",
        "tools.main.dns_bench": "DNS tests",
        "tools.main.rollback": "Atgriešana",
        "pci.main.modeln": "Modelis",
        "pci.main.batterystatus": "Statuss",
        "pmgr.main.status": "Statuss",
        "mon.main.1h": "1 h",
        "mon.main.7d": "7 d",
        "mon.main.30d": "30 d",
    },
}


def set_path(root: dict, path: str, value: str) -> None:
    parts = path.split(".")
    cur = root
    for part in parts[:-1]:
        cur = cur[part]
    cur[parts[-1]] = value


def main() -> None:
    for lang, fixes in FIXES.items():
        fp = LOC / f"{lang}.json"
        data = json.loads(fp.read_text(encoding="utf-8"))
        cats = data["categories"]
        for path, value in fixes.items():
            set_path(cats, path, value)
        fp.write_text(json.dumps(data, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
        print(f"Fixed {lang}: {len(fixes)} keys")


if __name__ == "__main__":
    main()
