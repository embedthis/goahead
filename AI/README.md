# GoAhead AI Documentation

This directory contains AI-assisted development documentation, context, and historical records for the GoAhead project.

## Purpose

The AI directory provides comprehensive documentation to assist AI tools (like Claude Code) in understanding the project structure, maintaining context across sessions, and preserving decision-making history.

## Directory Structure

### Active Documentation

- **[README-EJS.md](README-EJS.md)** - Complete guide for running tests with EJS+Bun runtime
  - Test setup and configuration
  - Examples and common issues
  - Quick reference for test development

### Plans

- **[plans/PLAN.md](plans/PLAN.md)** - Current project plan and roadmap
- **plans/** - Additional specific plans for features and improvements

### Designs

- **designs/DESIGN.md** - System architecture and design documentation
- **designs/** - Component-specific design documents

### Procedures

- **[procedures/PROCEDURE.md](procedures/PROCEDURE.md)** - Standard operating procedures
- **procedures/** - Specific procedures for development tasks

### Logs

- **[logs/CHANGELOG.md](logs/CHANGELOG.md)** - Official project changelog
  - Version 6.0.5: Test migration from Ejscript to TypeScript/EJS+Bun
  - Historical changes and security updates

- **[logs/FINAL_RESULTS.md](logs/FINAL_RESULTS.md)** - Test conversion final results
  - 33/34 tests passing (97% success rate)
  - Breakdown by category
  - Key technical changes

- **[logs/FINAL_CLEANUP_SUMMARY.md](logs/FINAL_CLEANUP_SUMMARY.md)** - Cleanup and optimization summary
  - Removed legacy files
  - Optimized imports
  - Final verification

### Archive

- **[archive/logs/](archive/logs/)** - Historical session logs and interim documents
  - Conversion notes and status reports
  - Intermediate summaries
  - Working documents from development sessions

## Recent Major Work

### Test Suite Migration (October 2025)

**Completed:** Full migration from native Ejscript (.tst.es) to TypeScript with EJS+Bun (.tst.ts)

**Key Achievements:**
- ✅ Converted all 34 unit tests
- ✅ 33/34 tests passing (97% success rate)
- ✅ 102/102 assertions passing
- ✅ Optimized imports using direct testme imports
- ✅ Removed legacy testme-globals.ts
- ✅ Comprehensive documentation

**Test Categories:**
- Basic HTTP: 14/14 tests ✓
- Authentication: 3/3 tests ✓
- Security: 4/4 tests ✓
- Stress: 7/7 tests ✓
- Other: 5/6 tests ✓ (1 IPv6 test skipped)

**Documentation:**
- [README-EJS.md](README-EJS.md) - Complete testing guide
- [logs/FINAL_RESULTS.md](logs/FINAL_RESULTS.md) - Detailed results
- [logs/CHANGELOG.md](logs/CHANGELOG.md) - Version history

## Using This Documentation

### For AI Tools

When working with GoAhead:

1. **Start here:** Read this README for project overview
2. **Check plans:** Review [plans/PLAN.md](plans/PLAN.md) for current objectives
3. **Understand design:** Read [designs/DESIGN.md](designs/DESIGN.md) for architecture
4. **Follow procedures:** Use [procedures/PROCEDURE.md](procedures/PROCEDURE.md) for workflows
5. **Log changes:** Update [logs/CHANGELOG.md](logs/CHANGELOG.md) and create session logs

### For Developers

- **Testing:** See [README-EJS.md](README-EJS.md) for running tests
- **Recent changes:** Check [logs/CHANGELOG.md](logs/CHANGELOG.md)
- **Project status:** Review [plans/PLAN.md](plans/PLAN.md)
- **Architecture:** Consult [designs/DESIGN.md](designs/DESIGN.md)

## Document Lifecycle

### Active Documents
Updated regularly, reflect current state:
- README.md (this file)
- README-EJS.md
- plans/PLAN.md
- logs/CHANGELOG.md

### Reference Documents
Created for specific work, remain stable:
- logs/FINAL_RESULTS.md
- logs/FINAL_CLEANUP_SUMMARY.md

### Archived Documents
Historical records moved to archive/:
- Conversion notes and interim status
- Session logs from completed work
- Working documents from development

## Maintenance

### When to Update

**README.md (this file):**
- New major features or changes
- Structural changes to AI directory
- Significant milestones

**README-EJS.md:**
- Test framework changes
- New test categories
- Updated requirements

**CHANGELOG.md:**
- Every release
- Security updates
- Bug fixes and features

**Archive:**
- Move completed session logs after merging
- Archive interim documents after final versions created
- Keep archive organized by date/topic

## Contributing

When working on GoAhead with AI assistance:

1. Review existing documentation in this directory
2. Create session logs as you work (AI/logs/SESSION-DATE.md)
3. Update CHANGELOG.md for user-facing changes
4. Archive completed work when done
5. Update plans and designs as needed

## Project Context

**GoAhead** is a small, fast, and secure embedded web server in maintenance mode.

**Status:** Actively maintained for security, not adding new features

**Migration Path:** [Ioto Device Agent](https://www.embedthis.com/ioto/)

**Security:** Compliant with EU Cyber Resilience Act requirements

## Resources

- **Main Project:** [/Users/mob/c/goahead/README.md](../README.md)
- **GoAhead Website:** https://www.embedthis.com/goahead/
- **Documentation:** https://www.embedthis.com/goahead/doc/
- **License:** See [LICENSE.md](../LICENSE.md)

---

**Last Updated:** 2025-10-27
