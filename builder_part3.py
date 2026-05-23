#!/usr/bin/env python3
# Part 3

part3 = """
<!-- ================================================= -->
<!-- STAGE 2: UNIVERSE - from Performance Page        -->
<!-- ================================================= -->
<g transform="translate(220, 80)">
    <rect x="0" y="0" width="120" height="155" rx="10" fill="rgba(59,130,246,0.04)" stroke="rgba(59,130,246,0.35)" stroke-width="1.5" stroke-dasharray="5,3"/>
    <text x="60" y="24" fill="#3b82f6" font-size="11" font-weight="700" text-anchor="middle">UNIVERSE</text>
    <text x="60" y="37" fill="#8892b0" font-size="8" text-anchor="middle" opacity="0.6">Note Range</text>

    <rect x="8" y="50" width="104" height="38" rx="6" fill="rgba(59,130,246,0.14)" stroke="#3b82f6" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="60" y="69" fill="#93c5fd" font-size="11" font-weight="700" text-anchor="middle">RANGE</text>
    <text x="60" y="83" fill="#3b82f6" font-size="8" text-anchor="middle" opacity="0.8">1-4 octaves</text>

    <rect x="8" y="95" width="104" height="38" rx="6" fill="rgba(59,130,246,0.14)" stroke="#3b82f6" stroke-width="1.5" filter="url(#cardShadow)"/>
    <text x="60" y="114" fill="#93c5fd" font-size="11" font-weight="700" text-anchor="middle">STEPS</text>
    <text x="60" y="128" fill="#3b82f6" font-size="8" text-anchor="middle" opacity="0.8">Steps Sieve</text>
</g>

<line x1="340" y1="157" x2="360" y2="157" stroke="#8892b0" stroke-width="1.5" opacity="0.4" marker-end="url(#arrow)"/>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'a') as f:
    f.write(part3)

print("Part 3 written")
