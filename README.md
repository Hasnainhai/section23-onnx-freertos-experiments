Start with Section23_Research_Report.pdf for the main findings.

Start with Section23_Research_Report.pdf for the main findings.
# Section 2.3 - Initial Experiments

Start with RESEARCH_NOTE.md for the findings.
See screenshots/05_research_architecture.svg for the architecture.
See REPRODUCE.md for build and execution instructions.
See STATIC_WCET_SCOPE_AND_LIMITATION.md for timing limitations.

Main evidence is in results/. Four terminal screenshots are
provided in screenshots/.

FreeRTOS kernel Git revision used:
640d0c43705e64cd53f46b31d882de6cd8433933

The full ONNX model runs on Linux CPU/GPU.
Only one isolated convolution channel runs on ARM FreeRTOS.

No validated hardware WCET or deterministic end-to-end
scheduling guarantee is claimed.
