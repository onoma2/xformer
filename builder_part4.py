#!/usr/bin/env python3
# Part 4

part4 = """
<!-- ================================================= -->
<!-- STAGE 3: TICKETS - from Scale Tickets Page       -->
<!-- ================================================= -->
<g transform="translate(380, 80)">
    <rect x="0" y="0" width="135" height="155" rx="10" fill="rgba(168,85,247,0.04)" stroke="rgba(168,85,247,0.35)" stroke-width="1.5" stroke-dasharray="5,3"/>
    <text x="67" y="24" fill="#a855f7" font-size="11" font-weight="700" text-anchor="middle">TICKETS</text>
    <text x="67" y="37" fill="#8892b0" font-size="8" text-anchor="middle" opacity="0.6">Note Weights</text>

    <rect x="8" y="47" width="119" height="36" rx="6" fill="rgba(168,85,247,0.14)" stroke="#a855f7" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="67" y="65" fill="#e9d5ff" font-size="10" font-weight="700" text-anchor="middle">DEGREE TICKETS</text>
    <text x="67" y="78" fill="#a855f7" font-size="8" text-anchor="middle" opacity="0.8">DEG 1-7, EXCL</text>

    <rect x="8" y="89" width="119" height="28" rx="6" fill="rgba(168,85,247,0.10)" stroke="rgba(168,85,247,0.6)" stroke-width="1" filter="url(#cardShadow)"/>
    <text x="67" y="107" fill="#e9d5ff" font-size="10" font-weight="600" text-anchor="middle">DROT</text>

    <rect x="8" y="122" width="119" height="28" rx="6" fill="rgba(168,85,247,0.10)" stroke="rgba(168,85,247,0.6)" stroke-width="1" filter="url(#cardShadow)"/>
    <text x="67" y="140" fill="#e9d5ff" font-size="10" font-weight="600" text-anchor="middle">MROT</text>
</g>

<line x1="515" y1="157" x2="535" y2="157" stroke="#8892b0" stroke-width="1.5" opacity="0.4" marker-end="url(#arrow)"/>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'a') as f:
    f.write(part4)

print("Part 4 written")
