#!/usr/bin/env python3
# Part 2

part2 = """
<!-- ================================================= -->
<!-- STAGE 1: KEY CONTEXT - from Config Page          -->
<!-- ================================================= -->
<g transform="translate(50, 80)">
    <rect x="0" y="0" width="130" height="155" rx="10" fill="rgba(78,205,196,0.04)" stroke="rgba(78,205,196,0.35)" stroke-width="1.5" stroke-dasharray="5,3"/>
    <text x="65" y="24" fill="#4ecdc4" font-size="11" font-weight="700" text-anchor="middle">CONFIG</text>
    <text x="65" y="37" fill="#8892b0" font-size="8" text-anchor="middle" opacity="0.6">Key Context</text>

    <rect x="8" y="47" width="114" height="27" rx="6" fill="rgba(78,205,196,0.14)" stroke="#4ecdc4" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="65" y="66" fill="#99f6e4" font-size="11" font-weight="700" text-anchor="middle">SCALE</text>

    <rect x="8" y="80" width="114" height="27" rx="6" fill="rgba(78,205,196,0.14)" stroke="#4ecdc4" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="65" y="99" fill="#99f6e4" font-size="11" font-weight="700" text-anchor="middle">ROOT NOTE</text>

    <rect x="8" y="113" width="114" height="24" rx="6" fill="rgba(78,205,196,0.10)" stroke="rgba(78,205,196,0.6)" stroke-width="1" filter="url(#cardShadow)"/>
    <text x="65" y="130" fill="#99f6e4" font-size="10" font-weight="600" text-anchor="middle">OCTAVE</text>

    <rect x="8" y="128" width="114" height="24" rx="6" fill="rgba(78,205,196,0.10)" stroke="rgba(78,205,196,0.6)" stroke-width="1" filter="url(#cardShadow)" transform="translate(0,14)"/>
    <text x="65" y="159" fill="#99f6e4" font-size="10" font-weight="600" text-anchor="middle">TRANSPOSE</text>
</g>

<!-- Flow arrows -->
<line x1="180" y1="157" x2="200" y2="157" stroke="#8892b0" stroke-width="1.5" opacity="0.4" marker-end="url(#arrow)"/>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'a') as f:
    f.write(part2)

print("Part 2 written")
