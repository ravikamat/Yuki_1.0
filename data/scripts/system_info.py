
#!/usr/bin/env python3
import sys, json
try:
    import psutil
    cpu = psutil.cpu_percent(interval=1)
    ram = psutil.virtual_memory()
    r = 'CPU: ' + str(cpu) + '% | RAM: ' + str(ram.percent) + '% used'
    print(json.dumps({'success': True, 'result': r}))
except Exception as e:
    print(json.dumps({'success': False, 'result': str(e)}))
