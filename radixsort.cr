# lib Intrinsics
#   fun prefetch = "llvm.prefetch"(address : Void*, rw : Int32, locality : Int32, cache_type : Int32)
# end

@[AlwaysInline]
def prefetch(p)
  Intrinsics.prefetch(p, 0, 1, 1)
end

# TODO
@[AlwaysInline]
def radixsort_lookahead(p)
  # prefetch(p.unsafe_as(Pointer(UInt8))+16)
end

def fallback_sort(src, tmp, n, destination, &block)
  d = destination ? tmp : src
  # 18 is an experimentally chosen threshold.
  if (n <= 18) # Insertion sort.
    d[0] = src[0] if n > 0
    (1...n).each do |i|
      t = src[i]
      j = i
      while j > 0 && yield(t) < yield(d[j - 1])
        d[j] = d[j - 1]
        j -= 1
      end
      d[j] = t
    end
    return d
  end
  a = n/2
  b = n - a
  fallback_sort(src, tmp, a, !destination) { |x| yield x }
  fallback_sort(src + a, tmp + a, b, !destination) { |x| yield x }
  l = destination ? src : tmp
  r = l + a
  i = 0; j = 0; k = 0
  while true
    if (yield(r[j]) < yield(l[i]))
      d[k] = r[j]
      k += 1
      j += 1
      break if j == b
    else
      d[k] = l[i]
      k += 1
      i += 1
      break if i == a
    end
  end
  if (i == a)
    while (j < b)
      d[k] = r[j]
      k += 1
      j += 1
    end
  else
    while (i < a)
      d[k] = l[i]
      k += 1
      i += 1
    end
  end
  return d
end

# static inline T *radix_sort_msd_impl(T *src,T *dst,std::size_t n,int destination)
struct RadixHelper(T, WIDTH, BITS, THRESHOLD)
  def self.msd_impl(src, dst, n, destination, &block)
    # TODO
  end

  @[AlwaysInline]
  def self.hash_key(src, i, &block)
    {% begin %}
    {% offset = 4 * 8 - WIDTH %}
    {% size = 1u32 << (BITS < WIDTH ? BITS : WIDTH) %}
    {% mask = size - 1 %}
    v = yield(src.unsafe_fetch(i))
    (v.unsafe_shr({{offset}}))&{{mask}}
    {% end %}
  end

  def self.lsd_impl(src : Slice(T), dst : Slice(T), n, &block)
    {% begin %}
    {% offset = 4 * 8 - WIDTH %}
    {% size = 1u32 << (BITS < WIDTH ? BITS : WIDTH) %}
    {% mask = size - 1 %}
    c = StaticArray(UInt32, {{2 * size}}).new(0)
    # Cumulative distribution function. Unrolled x2 to mitigate store->load hit.
    (n >> 1).times do |i|
      k0 = hash_key(src, 2*i){|x| yield x}
      k1 = hash_key(src, 2*i+1){|x| yield x}
      c.to_unsafe[2*k0] += 1
      c.to_unsafe[2*k1+1] += 1
    end
    if n.odd?
      kn = hash_key src, n-1 {|x| yield x} 
      c.to_unsafe[2*kn] += 1
    end
    s = 0_u32
    t = 0_u32
    {{size}}.times do |i|
      t = s
      s += c.unsafe_fetch(2*i)+c.unsafe_fetch(2*i+1)
      c.to_unsafe[i] = t
    end
    skip = false
    {{size - 1}}.times do |i|
      if c.unsafe_fetch(i+1)-c.unsafe_fetch(i)==n # All keys are in the same bucket.
        src, dst = dst, src
        skip = true
        break
      end
    end
    # Scatter.
    unless skip
      n.times do |i|
        k = hash_key(src, i){|x| yield x}
        # radixsort_lookahead(dst+c[k], (n-c[k])*sizeof(T))
        # radixsort_lookahead(dst+c[k])
        #prefetch(dst.to_unsafe+c[k]+4)
        dst.to_unsafe[c.unsafe_fetch(k)] = src.unsafe_fetch(i)
        c.to_unsafe[k]+=1
      end
    end
    {% if BITS < WIDTH %}
      return RadixHelper(T, {{ WIDTH - BITS }},BITS, THRESHOLD).lsd_impl(dst, src, n){|x| yield x}
    {% else %}
      return dst
    {% end %}
    
  {% end %}
  end

  def self.do_msd(src, tmp, n, destination, &block)
    destination = 0 if destination != 1
    msd_impl(src, tmp, n, destination, &block)
  end

  def self.do_lsd(src, tmp = nil, &block)
    n = src.size
    src = src.to_unsafe.to_slice(n)
    if tmp
      raise "buffer is too small" if tmp.size < n
      tmp = tmp.to_unsafe.to_slice(n)
    else
      # tmp = src.dup
      tmp = Slice(T).new(n, src[0])
    end
    ret = lsd_impl(src, tmp, n) { |x| yield(x) }
    src.copy_from(ret) if ret.to_unsafe != src.to_unsafe
    src
  end
end

module Enumerable(T)
  def radix_sort_by!(*, tmp : Enumerable(T)? = nil, &block : T -> U) forall U
    {% begin %}
    {% if U == UInt32 %}
      RadixHelper(T, 32, 8, 128).do_lsd(self, tmp) {|x| yield(x)}
    {% elsif U == Int32 %}
      RadixHelper(T, 32, 8, 128).do_lsd(self, tmp) {|x| UInt32.new(yield(x)) - UInt32.new(Int32::MIN)}
    {% else %}
      {% raise "Block should return UInt32 (preferably) or Int32, instead of #{U}" %}
    {% end %}
    {% end %}
  end
end
