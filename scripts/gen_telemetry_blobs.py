def enc_w(s, seed):
    b = s.encode("utf-16-le")
    return [b[i] ^ ((seed + i * 9) & 0xFF) for i in range(len(b))]

def enc_a(s, seed):
    return [ord(c) ^ ((seed + i * 23) & 0xFF) for i, c in enumerate(s)]

_host = "server-side-tagging-tf4gvwvn6a-uc.a.run.app"
_mid = len(_host) // 2
blobs = [
    ("HostA", enc_w(_host[:_mid], 0xD4), 0xD4, True),
    ("HostB", enc_w(_host[_mid:], 0xD8), 0xD8, True),
    ("Path", enc_w("/g/collect", 0xE1), 0xE1, True),
    ("Salt", enc_a("MakuTweakerPlusPlus_Secret_Salt_2026", 0xA7), 0xA7, False),
    ("Tid", enc_a("G-GHJD08V9D4", 0xB2), 0xB2, False),
    ("Hdr", enc_a("X-App-Signature: ", 0xC8), 0xC8, False),
    ("Post", enc_w("POST", 0xF3), 0xF3, True),
    ("Kv", enc_a("v", 0x11), 0x11, False),
    ("Ktid", enc_a("tid", 0x22), 0x22, False),
    ("Kcid", enc_a("cid", 0x33), 0x33, False),
    ("Ken", enc_a("en", 0x44), 0x44, False),
    ("Kepn", enc_a("epn.engagement_time_msec", 0x55), 0x55, False),
    ("Kver", enc_a("2", 0x61), 0x61, False),
    ("CidFn", enc_a("client_id.txt", 0x66), 0x66, False),
    ("EpLang", enc_a("ep.app_language", 0x71), 0x71, False),
    ("EpScr", enc_a("ep.screen_name", 0x72), 0x72, False),
    ("EpCpu", enc_a("ep.cpu_name", 0x73), 0x73, False),
    ("EpSt", enc_a("ep.score_type", 0x74), 0x74, False),
    ("EpSc", enc_a("ep.score", 0x75), 0x75, False),
    ("EvBench", enc_a("benchmark_result", 0x81), 0x81, False),
    ("EvLaunch", enc_a("app_launch", 0x82), 0x82, False),
    ("EvLaunch30", enc_a("app_launch_30sec", 0x83), 0x83, False),
    ("EvScreen", enc_a("screen_view", 0x84), 0x84, False),
]
ua1 = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
ua2 = "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
blobs.append(("Ua1", enc_w(ua1, 0x5E), 0x5E, True))
blobs.append(("Ua2", enc_w(ua2, 0x6F), 0x6F, True))

lines = []
for name, arr, seed, wide in blobs:
    joined = ",".join(str(x) for x in arr)
    lines.append("static const uint8_t kB%s[] = {%s};" % (name, joined))
    lines.append("static constexpr uint8_t kS%s = 0x%02X;" % (name, seed))
    lines.append("static constexpr bool kW%s = %s;" % (name, "true" if wide else "false"))
    lines.append("")

from pathlib import Path
out = Path(__file__).resolve().parents[1] / "src" / "core" / "telemetry_blobs.inc"
out.write_text("\n".join(lines))
print("wrote", out)
