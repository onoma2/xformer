#!/usr/bin/env python3
# Part 6 - EVOLUTION

part6 = """
<!-- ================================================= -->
<!-- STAGE 5: EVOLUTION - from Performance Page        -->
<!-- ================================================= -->
<g transform="translate(755, 80)">
    <rect x="0" y="0" width="115" height="155" rx="10" fill="rgba(233,69,96,0.04)" stroke="rgba(233,69,96,0.35)" stroke-width="1.5" stroke-dasharray="5,3"/>
    <text x="57" y="24" fill="#e94560" font-size="11" font-weight="700" text-anchor="middle">EVOLUTION</text>
    <text x="57" y="37" fill="#8892b0" font-size="8" text-anchor="middle" opacity="0.6">Randomness</text>

    <rect x="8" y="47" width="99" height="28" rx="6" fill="rgba(233,69,96,0.12)" stroke="#e94560" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="57" y="66" fill="#ff8fa3" font-size="10" font-weight="700" text-anchor="middle">VARIATION</text>

    <rect x="8" y="82" width="99" height="28" rx="6" fill="rgba(168,85,247,0.12)" stroke="#a855f7" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="57" y="101" fill="#e9d5ff" font-size="10" font-weight="700" text-anchor="middle">MUTATE</text>

    <rect x="8" y="117" width="99" height="28" rx="6" fill="rgba(168,85,247,0.10)" stroke="rgba(168,85,247,0.6)" stroke-width="1" filter="url(#cardShadow)"/>
    <text x="57" y="136" fill="#e9d5ff" font-size="9" font-weight="600" text-anchor="middle">PATIENCE M</text>
</g>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'a') as f:
    f.write(part6)

print("Part 6 written")
