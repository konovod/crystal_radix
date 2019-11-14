@[Link("radixsort_lib", ldflags: "-L#{__DIR__}")]
lib LibRadix
  # void radix_sort(unsigned int *src, unsigned int *tmp, unsigned int n)
  fun sort = radix_sort(src : UInt32*, tmp : UInt32*, n : UInt32) : Void
end
