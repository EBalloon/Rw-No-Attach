

template < typename Type >
Type Read(Type address, uint32_t offset) noexcept {

	if (address == Type(0)) {
		return Type(0);
	}

	return *reinterpret_cast<Type*>(uintptr_t(address) + offset);
}
