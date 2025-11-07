



namespace task {

using AffinityMask = uint64_t;

inline AffinityMask makeMask(const std::vector<int>& cores) {
    AffinityMask m = 0;
    for (int c : cores) if (c >= 0 && c < 64) m |= (AffinityMask(1) << c);
    return m;
}


} // namespace task