import json
import sys
import os

tmp_file = 'nvmeSpec.json'

os.system('nvme list -o json > ' + tmp_file)

with open(tmp_file) as json_file:

    data = json.load(json_file)
    for item in data['Devices']:
        if item['ModelNumber'] == sys.argv[1]:
            print(item['DevicePath'])

os.remove(tmp_file)
