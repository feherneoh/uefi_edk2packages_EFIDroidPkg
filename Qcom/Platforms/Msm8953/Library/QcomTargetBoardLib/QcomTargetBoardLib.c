#include <Base.h>
#include <Library/LKEnvLib.h>
#include <Chipset/board.h>
#include <Chipset/baseband.h>
#include <Chipset/smem.h>
#include <Library/QcomTargetBoardLib.h>

/* Detect the target type */
void target_detect(struct board_data *board)
{
	/* This is filled from board.c */
}

/* Detect the modem type */
void target_baseband_detect(struct board_data *board)
{
	uint32_t platform;

	platform = board->platform;

	switch(platform) {
	case MSM8953:
	case SDM450:
		board->baseband = BASEBAND_MSM;
		break;
	case APQ8053:
		board->baseband = BASEBAND_APQ;
		break;
	default:
		dprintf(CRITICAL, "Platform type: %u is not supported\n",platform);
		ASSERT(0);
	};
}
