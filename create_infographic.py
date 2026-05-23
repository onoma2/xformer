content = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Stochastic Pitch Infographic</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f0f23 100%);
            min-height: 100vh;
            padding: 30px 20px;
        }
        h1 { color: #e94560; font-size: 2rem; font-weight: 700; text-align: center; margin-bottom: 8px; }
        .subtitle { color: #8892b0; font-size: 1.1rem; text-align: center; margin-bottom: 30px; }
        .container { max-width: 1000px; margin: 0 auto; }
        .svg-wrapper {
            background: rgba(255,255,255,0.02);
            border-radius: 20px;
            padding: 20px;
            border: 1px solid rgba(255,255,255,0.08);
        }
        svg { width: 100%; height: auto; }
        .legend {
            display: flex;
            justify-content: center;
            gap: 25px;
            margin-top: 25px;
            flex-wrap: wrap;
        }
        .legend-item {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #8892b0;
            font-size: 0.85rem;
        }
        .legend-dot { width: 12px; height: 12px; border-radius: 50%; }
        .footer {
            color: #5a6a8a;
            font-size: 0.8rem;
            text-align: center;
            margin-top: 25px;
            line-height: 1.5;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>STOCHASTIC TRACK</h1>
        <p class="subtitle">What determines the PITCH of each note?</p>
        
        <div class="svg-wrapper">
'''

with open('/Users/foronoma/Work/Code/Eurorack/performer-phazer/create_infographic.py', 'w') as f:
    f.write(content)
print("Part 1 done")
