#!/usr/bin/env python3
# Part 1 of builder

part1 = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Stochastic Track: One Note Pitch</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #0f0f23 0%, #1a1a2e 50%, #16213e 100%);
            min-height: 100vh;
            padding: 40px 20px;
        }
        h1 { color: #e94560; font-size: 2.2rem; font-weight: 800; text-align: center; margin-bottom: 5px; letter-spacing: 1px; }
        .subtitle { color: #8892b0; font-size: 1.2rem; text-align: center; margin-bottom: 35px; font-weight: 400; }
        .container { max-width: 1100px; margin: 0 auto; }
        .svg-wrapper {
            background: rgba(255,255,255,0.02);
            border-radius: 24px;
            padding: 25px;
            border: 1px solid rgba(255,255,255,0.08);
            box-shadow: 0 25px 50px -12px rgba(0,0,0,0.6);
        }
        svg { width: 100%; height: auto; }
        .footer {
            color: #5a6a8a;
            font-size: 0.85rem;
            text-align: center;
            margin-top: 30px;
            line-height: 1.6;
            max-width: 850px;
            margin-left: auto;
            margin-right: auto;
        }
        .footer strong { color: #8892b0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>STOCHASTIC TRACK</h1>
        <p class="subtitle">What determines the PITCH of ONE NOTE?</p>
        <div class="svg-wrapper">
<svg viewBox="0 0 1000 720" xmlns="http://www.w3.org/2000/svg">
<defs>
    <linearGradient id="bg" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stop-color="#0f0f23"/>
        <stop offset="100%" stop-color="#1a1a2e"/>
    </linearGradient>
    <filter id="cardShadow">
        <feDropShadow dx="0" dy="3" stdDeviation="4" flood-color="#000" flood-opacity="0.4"/>
    </filter>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="#8892b0" opacity="0.5"/>
    </marker>
    <marker id="arrowRed" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto">
        <path d="M 0 0 L 10 5 L 0 10 z" fill="#e94560" opacity="0.9"/>
    </marker>
</defs>

<rect width="1000" height="720" fill="url(#bg)" rx="16"/>

<text x="500" y="36" fill="#e94560" font-size="15" font-weight="700" text-anchor="middle">ACTUAL UI CONTROL NAMES</text>
<text x="500" y="54" fill="#8892b0" font-size="10" text-anchor="middle" opacity="0.7">From Stochastic Config + Performance + Scale Tickets pages</text>
"""

output = "/Users/foronoma/Work/Code/Eurorack/performer-phazer/dola-stochastic-one-note-pitch.html"

with open(output, 'w') as f:
    f.write(part1)

print("Part 1 written")
