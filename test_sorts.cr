require "./radixsort_lib"
require "./radixsort"
require "benchmark"

def check_sorted(x, ref)
  check_sorted x, ref, &.itself
end

def check_sorted(x, ref, &block)
  # pp x, ref
  (1...x.size).each do |i|
    if yield(x[i]) != yield(ref[i])
      # pp! i, x[i - 1], x[i], x[i + 1], ref[i - 1], ref[i], ref[i + 1]
      pp! i, x[i], ref[i]
      raise "failed"
    end
  end
end

class SomeClass
  @name : String
  getter key : Int32

  def initialize(@key)
    @name = @key.to_s
  end

  def clone
    self.class.new(@key)
  end
end

struct SomeStruct
  @filler = StaticArray(UInt32, 1).new(0)
  getter key

  def initialize(@key : UInt32)
    @filler[0] = UInt32.new(@key)
  end

  def clone
    self.class.new(@key)
  end
end

def check_sanity(a)
  check_sanity(a, &.itself)
end

def check_sanity(a, &block)
  a.shuffle!
  b = a.clone
  10.times do
    a.shuffle!
    ref = a.sort_by { |x| yield x }
    a.radix_sort_by!(tmp: b) { |x| yield x }
    check_sorted(a, ref) { |x| yield x }
  end
  b.shuffle!
  return b
end

# {10, 100, 1000, 10000, 100000}.each do |n|
# n = 1000000
n = 100000

uint_a = Array(UInt32).new(n) { rand(UInt32::MAX) }
uint_ref = uint_a.sort
uint_b = check_sanity(uint_a, &.itself)
uint_a.shuffle!; uint_a.sort!; check_sorted uint_a, uint_ref
uint_a.shuffle!; LibRadix.sort(uint_a.to_unsafe, uint_b.to_unsafe, n); check_sorted uint_a, uint_ref
uint_a.shuffle!; uint_a.radix_sort_by!(&.itself); check_sorted uint_a, uint_ref

# int64_a = Array(Int64).new(n) { rand(Int64::MAX) }
# int64_b = check_sanity(int64_a, &.itself)

int_a = Array(Int32).new(n) { rand(Int32::MAX) }
int_b = check_sanity(int_a, &.itself)
int_ref = int_a.sort

struct_a = Array(SomeStruct).new(n) { SomeStruct.new(rand(UInt32::MAX)) }
struct_b = check_sanity(struct_a, &.key)

class_a = Array(SomeClass).new(n) { SomeClass.new(rand(Int32::MAX)) }
class_b = check_sanity(class_a, &.key)

Benchmark.ips do |x|
  x.report("#{n}: just shuffle") { uint_a.shuffle! }
  x.report("#{n}: stdlib sort") { uint_a.shuffle!; uint_a.sort! }
  x.report("#{n}: radix cpp") { uint_a.shuffle!; LibRadix.sort(uint_a.to_unsafe, uint_b.to_unsafe, n) }
  x.report("#{n}: crystal radix lsd") { uint_a.shuffle!; uint_a.radix_sort_by!(tmp: uint_b, &.itself) }
  x.report("#{n}: crystal radix lsd Int32") { int_a.shuffle!; int_a.radix_sort_by!(tmp: int_b, &.itself) }
  x.report("#{n}: crystal radix allocating") { uint_a.shuffle!; uint_a.radix_sort_by!(&.itself) }
  x.report("#{n}: crystal radix struct") { struct_a.shuffle!; struct_a.radix_sort_by!(tmp: struct_b, &.key) }
  x.report("#{n}: crystal radix class") { class_a.shuffle!; class_a.radix_sort_by!(tmp: class_b, &.key) }
end
