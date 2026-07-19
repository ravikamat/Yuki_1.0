import json

path = r'C:\Users\RahulRavi\.gemini\antigravity\brain\89d84593-b038-4083-b060-03426dfd2c5a\task.md'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

while content.startswith('"') and content.endswith('"'):
    try:
        content = json.loads(content)
    except:
        break

content = content.replace('\\n', '\n')

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
