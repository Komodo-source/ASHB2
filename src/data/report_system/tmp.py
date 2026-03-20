import csv

with open('../0.csv', newline='') as csvfile:
    reader = csv.reader(csvfile)
    for row in reader:
        print(row)