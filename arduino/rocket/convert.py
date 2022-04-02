import struct
import csv

with open("DATALOG.CSV", "w", newline="") as file_out:
    csv_out = csv.writer(file_out)
    csv_out.writerow(
        ("Millisecond", "XL_X", "XL_Y", "XL_Z", "G_X", "G_Y", "G_Z", "P", "T")
    )
    with open("DATALOG.DAT", mode="rb") as file_in:
        while True:
            chunk = file_in.read(36)
            if not chunk:
                break
            csv_out.writerow(
                [i if i == i else "" for i in struct.unpack("fffffffff", chunk)]
            )
