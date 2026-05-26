import json
from graphify.detect import detect
from pathlib import Path
result = detect(Path('.graphify-teletype-scope'))
print(json.dumps(result))
