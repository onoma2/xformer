#!/usr/bin/env python3
# Part 5 - SHAPING

part5 = """
<!-- ================================================= -->
<!-- STAGE 4: SHAPING - from Performance Page         -->
<!-- ================================================= -->
<g transform="translate(555, 80)">
    <rect x="0" y="0" width="160" height="155" rx="10" fill="rgba(59,130,246,0.04)" stroke="rgba(59,130,246,0.35)" stroke-width="1.5" stroke-dasharray="5,3"/>
    <text x="80" y="24" fill="#3b82f6" font-size="11" font-weight="700" text-anchor="middle">SHAPING</text>
    <text x="80" y="37" fill="#8892b0" font-size="8" text-anchor="middle" opacity="0.6">Movement</text>

    <g transform="translate(8, 47)">
        <rect x="0" y="0" width="70" height="28" rx="6" fill="rgba(59,130,246,0.14)" stroke="#3b82f6" stroke-width="1.5" filter="url(#cardShadow)"/>
        <text x="35" y="18" fill="#93c5fd" font-size="10" font-weight="700" text-anchor="middle">COMPLEX</text>

        <rect x="80" y="0" width="70" height="28" rx="6" fill="rgba(59,130,246,0.14)" stroke="#3b82f6" stroke-width="1.5" filter="url(#cardShadow)"/>
        <text x="115" y="18" fill="#93c5fd" font-size="10" font-weight="700" text-anchor="middle">CONTOUR</text>
    </g>

    <g transform="translate(8, 82)">
        <rect x="0" y="0" width="70" height="28" rx="6" fill="rgba(245,158,11,0.12)" stroke="#f59e0b" stroke-width="1.5" filter="url(#cardShadow)"/>
        <text x="35" y="18" fill="#fcd34d" font-size="10" font-weight="700" text-anchor="middle">BIAS</text>

        <rect x="80" y="0" width="70" height="28" rx="6" fill="rgba(245,158,11,0.12)" stroke="#f59e0b" stroke-width="1.5" filter="url(#cardShadow)"/>
        <text x="115" y="18" fill="#fcd34d" font-size="10" font-weight="700" text-anchor="middle">SPREAD</text>
    </g>

    <g transform="translate(8, 117)">
        <rect x="0" y="0" width="144" height="28" rx="6" fill="rgba(16,185,129,0.10)" stroke="rgba(16,185,129,0.6)" stroke-width="1" filter="url(#cardShadow)"/>
        <text x="72" y="18" fill="#34d399" font-size="10" font-weight="600" text-anchor="middle">REPEAT</text>
    </g>
</g>

<line x1="715" y1="157" x2="735" y2="157" stroke="#8892b0" stroke-width="1.5" opacity="0.4" marker-end="url(#arrow)"/>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'a') as f:
    f.write(part5)

print("Part 5 written")
