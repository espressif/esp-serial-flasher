# Contributing to ESP Serial Flasher

We welcome contributions to the ESP Serial Flasher project! This guide will help you get started with contributing.

## Before You Contribute

**Please open an issue first** to discuss whether your proposed change could be accepted. This helps ensure that:
- Your contribution aligns with the project's goals
- You don't spend time on changes that might not be merged
- We can provide guidance on the best implementation approach

## Types of Contributions

### Bug Reports
- Use [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- Check if the issue has already been reported
- Provide detailed reproduction steps
- Include relevant system information (OS, toolchain versions, etc.)

### Feature Requests
- Use [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- Describe the use case and motivation
- Explain the expected behavior

### Pull Requests
- Follow the guidelines below for code contributions
- Reference the related issue in your PR description

## Development Guidelines

### Code Style
Follow the [ESP-IDF project's contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html), which include:

- Code formatting standards
- Naming conventions
- Documentation requirements

### Commit Messages
Use [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) format:

```
type(scope): description

[optional body]

[optional footer(s)]
```

Examples:
```
feat(uart): add support for custom baud rates
fix(spi): resolve timing issue with fast flash chips
docs(readme): update installation instructions
```

## Setting Up Development Environment

### Install Pre-commit Hooks

We use [pre-commit](https://pre-commit.com/) to automatically enforce code quality standards:

```bash
pip install pre-commit
pre-commit install
pre-commit install -t commit-msg
```

This will automatically run linters and formatters on your code before each commit.

### Available Pre-commit Hooks
- **Trailing whitespace removal**
- **End-of-file fixing**
- **Line ending normalization**
- **Code formatting** (astyle)
- **Commit message validation**
- **Python code formatting** (ruff)
- **Spell checking** (codespell)

## Testing Your Changes

### Build Testing
Ensure your changes build successfully on relevant platforms:

```bash
# Example for ESP-IDF
cd examples/esp32_example
idf.py build

# Example for STM32 (adjust paths as needed)
mkdir build && cd build
cmake -DPORT=STM32 -DSTM32_CHIP=STM32F407VG ..
cmake --build .
```

### Functional Testing
- Test your changes with actual hardware when possible
- Verify that existing functionality still works
- Add tests for new features when applicable

## Adding Support for New Platforms

If you want to add support for a new host platform, please see our [Supporting New Targets Guide](supporting-new-targets.md).

## Documentation

### Updating Documentation
- Update relevant documentation files when making changes
- Follow the [Espressif Manual of Style](https://mos.espressif.com/)
- Ensure all code examples are tested and working

### Documentation Structure
- Main README: Overview and getting started
- `docs/`: Detailed documentation
- Example READMEs: Platform-specific examples

## Review Process

### What to Expect
1. **Automated checks**: Pre-commit hooks and CI will run
2. **Code review**: Maintainers will review your code
3. **Testing**: Your changes may be tested on various platforms
4. **Feedback**: You may be asked to make changes

### Timeline
- Initial review: Usually within a few days
- Follow-up: Depends on complexity and reviewer availability

## Getting Help

### Where to Ask Questions
- **General questions**: [GitHub Discussions](https://github.com/espressif/esp-serial-flasher/discussions) (if available)
- **Bug reports**: [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- **Development questions**: Open an issue with the "question" label

### Additional Resources
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Espressif Manual of Style](https://mos.espressif.com/)
- [Conventional Commits](https://www.conventionalcommits.org/)

## License

By contributing to this project, you agree that your contributions will be licensed under the same [Apache 2.0 License](../LICENSE) that covers the project.