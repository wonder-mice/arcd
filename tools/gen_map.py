#!/usr/bin/python

import sys
import argparse

def partial_sums(a):
	return reduce(lambda c, x: c + [c[-1] + x], a, [0])[1:]

def transpose(a):
	return zip(*a)

def symbol(i):
	if 0 > i: return "-"
	if 10 > i: return str(i)
	if ord('z') - ord('a') + 10 >= i:
		return chr(i - 10 + ord('a'))
	return "#"

def gen_binary(n):
	if 0 == n: return []
	if 1 == n: return [[0, 1]]
	binary = gen_binary(n - 1)
	m = len(binary[0])
	return [[0] * m + [1] * m] + map(lambda v: 2*v, binary)

def gen_intervals(model, length):
	n = sum(model)
	ch = 0
	lo = 0
	intervals = []
	for cprob in partial_sums(model):
		hi = length * cprob / n
		interval = [symbol(ch) * (hi - lo)]
		if n <= hi - lo:
			interval += gen_intervals(model, hi - lo)
		intervals.append(interval)
		lo = hi
		ch += 1
	m = len(max(intervals, key=lambda x: len(x)))
	for interval in intervals:
		while len(interval) < m:
			interval.append(symbol(-1) * len(interval[0]))
	return map(lambda x: "".join(x), transpose(intervals))

def printable(line):
	return "".join(map(lambda v: str(v), line))

def main(argv):
	parser = argparse.ArgumentParser(description="Generates range coder map.")
	parser.add_argument("-m", "--model", metavar='MODEL', required=True,
			help="Probabilities, example: \"1, 2, 4\"")
	parser.add_argument("-b", "--bits", metavar='N', type=int, required=True,
			help="Bit count")
	args = parser.parse_args(argv)

	model = map(lambda v: int(v.strip()), args.model.split(","))
	binary = gen_binary(args.bits)
	intervals = gen_intervals(model, len(binary[0]))
	for line in intervals:
		print printable(line)
	for line in binary:
		print printable(line)
	return 0

if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
