#!/usr/bin/python
import glob
import sys

def main():
  _, to_glob, out_path, depfile_path = sys.argv
  to_concat = glob.glob(to_glob)
  outfile = open(out_path, "w")
  depfile = open(depfile_path, "w")
  depfile.write(out_path)
  depfile.write(":")
  for input_file in sorted(to_concat):
    with open(input_file, "r") as infile:
      for line in infile:
        outfile.write(line)
    depfile.write(" ")
    depfile.write(input_file)
  depfile.write("\n")

if __name__ == '__main__':
  main()
