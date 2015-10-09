#!/usr/bin/python

import sys

def partial_sums(a):
	return reduce(lambda c, x: c + [c[-1] + x], a, [0])[1:]

def gen_binary(n):
	if 0 == n: return []
	if 1 == n: return [[0, 1]]
	binary = gen_binary(n - 1)
	m = len(binary[0])
	return [[0] * m + [1] * m] + map(lambda v: 2*v, binary)

def gen_cprobs(model):
	return partial_sums(model)

def gen_intervals(model, length):
	n = sum(model)
	cprobs = partial_sums(model)
	lo = 0
	s = []
	i = 0
	for p in cprobs:
		hi = length * p / n
		for k in range(lo, hi):
			s.append(i)
		i += 1
		lo = hi
	return s

def main(argv):
	model = map(lambda v: int(v.strip()), argv[0].split(","))
	print "Model:", model
	b = gen_binary(6)
	for line in b:
		print "".join(map(lambda v: str(v), line))
	intervals = gen_intervals(model, len(b[0]))
	print "".join(map(lambda v: str(v), intervals))

	return 0

if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
