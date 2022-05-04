import json
import sys
import os

tmp_file = 'nvmeSpec.out'

os.system('nvme list -o json > ' + tmp_file)

with open(tmp_file) as json_file:

    data = json.load(json_file)
    for item in data['Devices']:
        if item['DevicePath'] == sys.argv[1]:
            print(item['ModelNumber'])
            if item.get('PhysicalSize') is not None:
                print(item['PhysicalSize'])
            else:
                out = os.popen('fdisk -l | grep '+sys.argv[1]).read()
                for field in out.split(','):
                    if 'bytes' in field:
                        print(field.replace('bytes', '').strip())
                        break

os.remove(tmp_file)
