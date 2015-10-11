#!/usr/bin/python

import sys
import argparse

def partial_sums(a):
	return reduce(lambda c, x: c + [c[-1] + x], a, [0])[1:]

def transpose(a):
	return zip(*a)

def join(a):
	return reduce(lambda c, x: c + x, a, [])

def indexof(a, b, e, f):
	for i in range(b, e):
		if f(a[i]): return i
	return e

def str_to_list(s):
	return map(lambda v: int(v.strip()), s.split(","))

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
		interval = [[ch] * (hi - lo)]
		if n <= hi - lo:
			interval += gen_intervals(model, hi - lo)
		intervals.append(interval)
		lo = hi
		ch += 1
	m = len(max(intervals, key=lambda x: len(x)))
	for interval in intervals:
		while len(interval) < m:
			interval.append([-1] * len(interval[0]))
	return map(join, transpose(intervals))

def encode_range(intervals, binary, sequence):
	n = len(intervals[0])
	lo, hi = 0, n
	for i in range(len(sequence)):
		ch = sequence[i]
		if i >= len(intervals):
			return (-1, (lo, hi))
		line = intervals[i]
		sublo = indexof(line, lo, hi, lambda x: ch == x)
		subhi = indexof(line, sublo, hi, lambda x: ch != x)
		if sublo == subhi:
			return (-1, (lo, hi))
		lo = sublo
		hi = subhi
	for i in range(len(binary)):
		line = binary[i]
		sublo = hi
		subhi = lo
		for k in range(lo, hi):
			if 0 == k or line[k - 1] != line[k]:
				sublo = k
				break
		for k in range(hi, lo, -1):
			if n == k or line[k - 1] != line[k]:
				subhi = k
				break
		if sublo < subhi:
			lo = sublo
			hi = subhi
			return (i + 1, (lo, hi))
	return (-1, (lo, hi))

def cut_width(intervals, binary, marks, width):
	length = len(binary[0])
	lo, hi = marks[0], marks[-1]
	if lo < width: lo = 0
	else: lo -= width
	if hi + width >= length: hi = length
	else: hi += width
	intervals = map(lambda x: x[lo:hi], intervals)
	binary = map(lambda x: x[lo:hi], binary)
	marks = map(lambda x: x - lo, marks)
	return intervals, binary, marks

def list_to_str(line, marks=None, mark="|"):
	xs = map(lambda v: symbol(v), line)
	if marks is not None:
		for i in reversed(marks):
			xs.insert(i, mark)
	return "".join(xs)

def print_map(intervals, binary, marks, bits):
	length = len(binary[0]) + len(marks)
	for i in range(len(intervals)):
		print list_to_str(intervals[i], marks)
	print "=" * length
	for i in range(len(binary)):
		if bits == i:
			print "~" * length
		print list_to_str(binary[i], marks)

def main(argv):
	parser = argparse.ArgumentParser(description="Generates range coder map.")
	parser.add_argument("-m", "--model", metavar="MODEL", required=True,
			help="Probabilities, example: \"1, 2, 4\"")
	parser.add_argument("-b", "--bits", metavar="N", type=int, required=True,
			help="Bit count")
	parser.add_argument("-e", "--encode", metavar="SEQUENCE", default=None,
			help="Sequence to encode, example: \"0, 2, 1\"")
	parser.add_argument("-w", "--width", metavar="WIDTH", type=int, default=None,
			help="Width of output around found interval")
	args = parser.parse_args(argv)

	if 0 >= args.bits:
		print >> sys.stderr, "Error: bit count must be > 0"
		return 1
	model = str_to_list(args.model)
	binary = gen_binary(args.bits)
	intervals = gen_intervals(model, len(binary[0]))
	encode_marks = None
	encode_bits = None
	if args.encode is not None:
		sequence = str_to_list(args.encode)
		encode_bits, encode_marks = encode_range(intervals, binary, sequence)
		if 0 > encode_bits:
			print >> sys.stderr, "Warning: not enough bits to fully encode the sequence"

	if encode_marks is not None and args.width is not None:
		intervals, binary, encode_marks = cut_width(intervals, binary, encode_marks, args.width)
	print_map(intervals, binary, encode_marks, encode_bits)
	return 0

if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
