# Contributing to APM System

Thanks for your interest in contributing! This guide will help you get started.

## Quick Start

1. **Fork and clone**
   ```bash
   git clone https://github.com/your-username/apm-system.git
   cd apm-system
   ```

2. **Build and test**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
   make -j$(nproc)
   ctest --output-on-failure
   ```

3. **Create a branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Code Standards

### Style Guide
- **C++ Standard:** C++17
- **Formatting:** Follow existing code style (2 spaces, no tabs)
- **Naming:**
  - Classes: `PascalCase`
  - Functions/variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Private members: `trailing_underscore_`

### Before Committing
- Run tests: `ctest`
- Format code: Use clang-format if available
- Check for warnings: Compile with `-Wall -Wextra`

## Pull Request Process

1. **Keep PRs focused** - One feature/fix per PR
2. **Write clear commits** - Use imperative mood: "Add feature" not "Added feature"
3. **Add tests** - New features need tests
4. **Update docs** - Document new APIs and features
5. **Pass CI** - All tests must pass

### Commit Message Format
```
Short summary (50 chars or less)

Detailed explanation if needed. Wrap at 72 characters.

- Bullet points are okay
- Reference issues: Fixes #123
```

## What to Contribute

### Good First Issues
- Documentation improvements
- Test coverage additions
- Bug fixes with clear reproduction steps
- Performance optimizations with benchmarks

### Bigger Features
- Discuss in an issue first
- Break into smaller PRs when possible
- Follow the roadmap priorities

## Testing

### Unit Tests
```bash
# Run all tests
ctest

# Run specific test
./apm_tests --gtest_filter=BeamformerTest.*
```

### Integration Tests
```bash
# Test with real audio
./apm --preset "Conference Room" --input test_audio.wav
```

### Benchmarks
```bash
./apm_benchmarks --benchmark_filter=BM_DelayAndSum
```

## Code Review

- Be respectful and constructive
- Reviews typically take 1-3 days
- Address feedback promptly
- Don't take criticism personally

## Need Help?

- 💬 **Questions:** Open a discussion
- 🐛 **Bugs:** File an issue with reproduction steps
- 💡 **Feature Ideas:** Open an issue to discuss first
- 📧 **Private concerns:** Email maintainers

## License

By contributing, you agree your contributions will be licensed under the MIT License.

---

**Ready to contribute?** Pick an issue labeled `good first issue` or `help wanted`!
