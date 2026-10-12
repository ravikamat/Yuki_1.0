import re
with open('F:/Yuki_1.0/config/cognitive_config.fbs', 'r') as f:
    content = f.read()

# Add quotes around property names
content = re.sub(r'(\s+)(\w+):', r'\1"\2":', content)

with open('F:/Yuki_1.0/config/cognitive_config.json', 'w') as f:
    f.write(content)

print('Converted to JSON')