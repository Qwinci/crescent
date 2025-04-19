#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "mem/iospace.hpp"
#include "mem/mem.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BitRegister<u32> FUSE_STATUS {0x42000};
	static constexpr BitRegister<u32> TC_HOTPLUG_CTL {0x44038};
	static constexpr BitRegister<u32> DISPLAY_INT_CTL {0x44200};
	static constexpr BitRegister<u32> DE_PIPE_ISR_A {0x44400};
	static constexpr BitRegister<u32> DE_PIPE_IMR_A {0x44404};
	static constexpr BitRegister<u32> DE_PIPE_IRR_A {0x44408};
	static constexpr BitRegister<u32> DE_PIPE_IER_A {0x4440C};
	static constexpr BitRegister<u32> DE_PIPE_ISR_B {0x44410};
	static constexpr BitRegister<u32> DE_PIPE_IMR_B {0x44414};
	static constexpr BitRegister<u32> DE_PIPE_IRR_B {0x44418};
	static constexpr BitRegister<u32> DE_PIPE_IER_B {0x4441C};
	static constexpr BitRegister<u32> DE_PIPE_ISR_C {0x44420};
	static constexpr BitRegister<u32> DE_PIPE_IMR_C {0x44424};
	static constexpr BitRegister<u32> DE_PIPE_IRR_C {0x44428};
	static constexpr BitRegister<u32> DE_PIPE_IER_C {0x4442C};
	static constexpr BitRegister<u32> DE_PIPE_ISR_D {0x44430};
	static constexpr BitRegister<u32> DE_PIPE_IMR_D {0x44434};
	static constexpr BitRegister<u32> DE_PIPE_IRR_D {0x44438};
	static constexpr BitRegister<u32> DE_PIPE_IER_D {0x4443C};
	static constexpr BitRegister<u32> DE_HPD_ISR {0x44470};
	static constexpr BitRegister<u32> DE_HPD_IMR {0x44474};
	static constexpr BitRegister<u32> DE_HPD_IIR {0x44478};
	static constexpr BitRegister<u32> DE_HPD_IER {0x4447C};
	static constexpr BitRegister<u32> DBUF_CTL_S2 {0x44FE8};
	static constexpr BitRegister<u32> DBUF_CTL_S1 {0x45008};
	static constexpr BitRegister<u32> MBUS_ABOX_CTL {0x45038};
	static constexpr BitRegister<u32> MBUS_ABOX1_CTL {0x45048};
	static constexpr BitRegister<u32> MBUS_ABOX2_CTL {0x4504C};
	static constexpr BitRegister<u32> BW_BUDDY1_CTL {0x45140};
	static constexpr BitRegister<u32> BW_BUDDY2_CTL {0x45150};
	static constexpr BitRegister<u32> PWR_WELL_CTL2 {0x45404};
	static constexpr BitRegister<u32> PWR_WELL_CTL_AUX2 {0x45444};
	static constexpr BitRegister<u32> PWR_WELL_CTL_DDI2 {0x45454};
	static constexpr BitRegister<u32> CDCLK_CTL {0x46000};
	static constexpr BitRegister<u32> DPLL0_ENABLE {0x46010};
	static constexpr BitRegister<u32> DPLL1_ENABLE {0x46014};
	static constexpr BitRegister<u32> NDE_RSTWRN_OPT {0x46408};
	static constexpr BitRegister<u32> CDCLK_PLL_ENABLE {0x46070};
	static constexpr BitRegister<u32> TRANS_CLK_SEL_A {0x46140};
	static constexpr BitRegister<u32> TRANS_CLK_SEL_B {0x46144};
	static constexpr BitRegister<u32> TRANS_CLK_SEL_C {0x46148};
	static constexpr BitRegister<u32> TRANS_CLK_SEL_D {0x4614C};
	static constexpr BitRegister<u32> DSSM {0x51004};
	static constexpr BitRegister<u32> TRANS_HTOTAL_A {0x60000};
	static constexpr BitRegister<u32> TRANS_HBLANK_A {0x60004};
	static constexpr BitRegister<u32> TRANS_HSYNC_A {0x60008};
	static constexpr BitRegister<u32> TRANS_VTOTAL_A {0x6000C};
	static constexpr BitRegister<u32> TRANS_VBLANK_A {0x60010};
	static constexpr BitRegister<u32> TRANS_VSYNC_A {0x60014};
	static constexpr BitRegister<u32> PIPE_SRCSZ_A {0x6001C};
	static constexpr BitRegister<u32> TRANS_SPACE_A {0x60024};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_A {0x60028};
	static constexpr BitRegister<u32> TRANS_MULT_A {0x6002C};
	static constexpr BitRegister<u32> TRANS_DDI_FUNC_CTL_A {0x60400};
	static constexpr BitRegister<u32> TRANS_MSA_MISC_A {0x60410};
	static constexpr BitRegister<u32> DP_TP_CTL_A {0x60540};
	static constexpr BitRegister<u32> DP_TP_STATUS_A {0x60544};
	static constexpr BitRegister<u32> TRANS_PUSH_A {0x60A70};
	static constexpr BitRegister<u32> TRANS_HTOTAL_B {0x61000};
	static constexpr BitRegister<u32> TRANS_HBLANK_B {0x61004};
	static constexpr BitRegister<u32> TRANS_HSYNC_B {0x61008};
	static constexpr BitRegister<u32> TRANS_VTOTAL_B {0x6100C};
	static constexpr BitRegister<u32> TRANS_VBLANK_B {0x61010};
	static constexpr BitRegister<u32> TRANS_VSYNC_B {0x61014};
	static constexpr BitRegister<u32> PIPE_SRCSZ_B {0x6101C};
	static constexpr BitRegister<u32> TRANS_SPACE_B {0x61024};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_B {0x61028};
	static constexpr BitRegister<u32> TRANS_MULT_B {0x6102C};
	static constexpr BitRegister<u32> TRANS_DDI_FUNC_CTL_B {0x61400};
	static constexpr BitRegister<u32> TRANS_MSA_MISC_B {0x61410};
	static constexpr BitRegister<u32> DP_TP_CTL_B {0x61540};
	static constexpr BitRegister<u32> DP_TP_STATUS_B {0x61544};
	static constexpr BitRegister<u32> TRANS_PUSH_B {0x61A70};
	static constexpr BitRegister<u32> TRANS_HTOTAL_C {0x62000};
	static constexpr BitRegister<u32> TRANS_HBLANK_C {0x62004};
	static constexpr BitRegister<u32> TRANS_HSYNC_C {0x62008};
	static constexpr BitRegister<u32> TRANS_VTOTAL_C {0x6200C};
	static constexpr BitRegister<u32> TRANS_VBLANK_C {0x62010};
	static constexpr BitRegister<u32> TRANS_VSYNC_C {0x62014};
	static constexpr BitRegister<u32> PIPE_SRCSZ_C {0x6201C};
	static constexpr BitRegister<u32> TRANS_SPACE_C {0x62024};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_C {0x62028};
	static constexpr BitRegister<u32> TRANS_MULT_C {0x6202C};
	static constexpr BitRegister<u32> TRANS_DDI_FUNC_CTL_C {0x62400};
	static constexpr BitRegister<u32> TRANS_MSA_MISC_C {0x62410};
	static constexpr BitRegister<u32> DP_TP_CTL_C {0x62540};
	static constexpr BitRegister<u32> DP_TP_STATUS_C {0x62544};
	static constexpr BitRegister<u32> TRANS_PUSH_C {0x62A70};
	static constexpr BitRegister<u32> TRANS_HTOTAL_D {0x63000};
	static constexpr BitRegister<u32> TRANS_HBLANK_D {0x63004};
	static constexpr BitRegister<u32> TRANS_HSYNC_D {0x63008};
	static constexpr BitRegister<u32> TRANS_VTOTAL_D {0x6300C};
	static constexpr BitRegister<u32> TRANS_VBLANK_D {0x63010};
	static constexpr BitRegister<u32> TRANS_VSYNC_D {0x63014};
	static constexpr BitRegister<u32> PIPE_SRCSZ_D {0x6301C};
	static constexpr BitRegister<u32> TRANS_SPACE_D {0x63024};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_D {0x63028};
	static constexpr BitRegister<u32> TRANS_MULT_D {0x6302C};
	static constexpr BitRegister<u32> TRANS_DDI_FUNC_CTL_D {0x63400};
	static constexpr BitRegister<u32> TRANS_MSA_MISC_D {0x63410};
	static constexpr BitRegister<u32> DP_TP_CTL_D {0x63540};
	static constexpr BitRegister<u32> DP_TP_STATUS_D {0x63544};
	static constexpr BitRegister<u32> TRANS_PUSH_D {0x63A70};
	static constexpr BitRegister<u32> DDI_BUF_CTL_A {0x64000};
	static constexpr BitRegister<u32> DDI_AUX_CTL_A {0x64010};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_A {0x64014};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_A {0x64018};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_A {0x6401C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_A {0x64020};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_A {0x64024};
	static constexpr BitRegister<u32> DDI_BUF_CTL_B {0x64100};
	static constexpr BitRegister<u32> DDI_AUX_CTL_B {0x64110};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_B {0x64114};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_B {0x64118};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_B {0x6411C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_B {0x64120};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_B {0x64124};
	static constexpr BitRegister<u32> DDI_BUF_CTL_C {0x64200};
	static constexpr BitRegister<u32> DDI_AUX_CTL_C {0x64210};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_C {0x64214};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_C {0x64218};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_C {0x6421C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_C {0x64220};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_C {0x64224};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC1 {0x64300};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC1 {0x64310};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC1 {0x64314};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC1 {0x64318};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC1 {0x6431C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC1 {0x64320};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC1 {0x64324};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC2 {0x64400};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC2 {0x64410};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC2 {0x64414};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC2 {0x64418};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC2 {0x6441C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC2 {0x64420};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC2 {0x64424};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC3 {0x64500};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC3 {0x64510};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC3 {0x64514};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC3 {0x64518};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC3 {0x6451C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC3 {0x64520};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC3 {0x64524};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC4 {0x64600};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC4 {0x64610};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC4 {0x64614};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC4 {0x64618};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC4 {0x6461C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC4 {0x64620};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC4 {0x64624};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC5 {0x64700};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC5 {0x64710};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC5 {0x64714};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC5 {0x64718};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC5 {0x6471C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC5 {0x64720};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC5 {0x64724};
	static constexpr BitRegister<u32> DDI_BUF_CTL_USBC6 {0x64800};
	static constexpr BitRegister<u32> DDI_AUX_CTL_USBC6 {0x64810};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_0_USBC6 {0x64814};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_1_USBC6 {0x64818};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_2_USBC6 {0x6481C};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_3_USBC6 {0x64820};
	static constexpr BasicRegister<u32> DDI_AUX_DATA_4_USBC6 {0x64824};
	static constexpr BitRegister<u32> PHY_MISC_A {0x64C00};
	static constexpr BitRegister<u32> PHY_MISC_B {0x64C04};
	static constexpr BitRegister<u32> PHY_MISC_C {0x64C08};
	static constexpr BitRegister<u32> TRANS_HTOTAL_DSI0 {0x6B000};
	static constexpr BitRegister<u32> TRANS_HSYNC_DSI0 {0x6B008};
	static constexpr BitRegister<u32> TRANS_VTOTAL_DSI0 {0x6B00C};
	static constexpr BitRegister<u32> TRANS_VBLANK_DSI0 {0x6B010};
	static constexpr BitRegister<u32> TRANS_VSYNC_DSI0 {0x6B014};
	static constexpr BitRegister<u32> TRANS_SPACE_DSI0 {0x6B024};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_DSI0 {0x6B028};
	static constexpr BitRegister<u32> TRANS_HTOTAL_DSI1 {0x6B800};
	static constexpr BitRegister<u32> TRANS_HSYNC_DSI1 {0x6B808};
	static constexpr BitRegister<u32> TRANS_VTOTAL_DSI1 {0x6B80C};
	static constexpr BitRegister<u32> TRANS_VBLANK_DSI1 {0x6B810};
	static constexpr BitRegister<u32> TRANS_VSYNC_DSI1 {0x6B814};
	static constexpr BitRegister<u32> TRANS_SPACE_DSI1 {0x6B824};
	static constexpr BitRegister<u32> TRANS_VSYNCSHIFT_DSI1 {0x6B828};
	static constexpr BitRegister<u32> PORT_CL_DW5_B {0x6C014};
	static constexpr BitRegister<u32> PORT_CL_DW10_B {0x6C028};
	static constexpr BitRegister<u32> PORT_COMP_DW0_B {0x6C100};
	static constexpr BitRegister<u32> PORT_COMP_DW1_B {0x6C104};
	static constexpr BitRegister<u32> PORT_COMP_DW3_B {0x6C10C};
	static constexpr BitRegister<u32> PORT_COMP_DW9_B {0x6C124};
	static constexpr BitRegister<u32> PORT_COMP_DW10_B {0x6C128};
	static constexpr BitRegister<u32> PORT_PCS_DW1_AUX_B {0x6C304};
	static constexpr BitRegister<u32> PORT_TX_DW2_AUX_B {0x6C388};
	static constexpr BitRegister<u32> PORT_TX_DW4_AUX_B {0x6C390};
	static constexpr BitRegister<u32> PORT_TX_DW5_AUX_B {0x6C394};
	static constexpr BitRegister<u32> PORT_TX_DW7_AUX_B {0x6C39C};
	static constexpr BitRegister<u32> PORT_TX_DW8_AUX_B {0x6C3A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_GRP_B {0x6C604};
	static constexpr BitRegister<u32> PORT_TX_DW2_GRP_B {0x6C688};
	static constexpr BitRegister<u32> PORT_TX_DW4_GRP_B {0x6C690};
	static constexpr BitRegister<u32> PORT_TX_DW5_GRP_B {0x6C694};
	static constexpr BitRegister<u32> PORT_TX_DW7_GRP_B {0x6C69C};
	static constexpr BitRegister<u32> PORT_TX_DW8_GRP_B {0x6C6A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_LN0_B {0x6C804};
	static constexpr BitRegister<u32> PORT_TX_DW2_LN0_B {0x6C888};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN0_B {0x6C890};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN0_B {0x6C894};
	static constexpr BitRegister<u32> PORT_TX_DW7_LN0_B {0x6C89C};
	static constexpr BitRegister<u32> PORT_TX_DW8_LN0_B {0x6C8A0};
	static constexpr BitRegister<u32> PORT_TX_DW2_LN1_B {0x6C988};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN1_B {0x6C990};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN1_B {0x6C994};
	static constexpr BitRegister<u32> PORT_TX_DW7_LN1_B {0x6C99C};
	static constexpr BitRegister<u32> PORT_TX_DW2_LN2_B {0x6CA88};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN2_B {0x6CA90};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN2_B {0x6CA94};
	static constexpr BitRegister<u32> PORT_TX_DW7_LN2_B {0x6CA9C};
	static constexpr BitRegister<u32> PORT_TX_DW2_LN3_B {0x6CB88};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN3_B {0x6CB90};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN3_B {0x6CB94};
	static constexpr BitRegister<u32> PORT_TX_DW7_LN3_B {0x6CB9C};
	static constexpr BitRegister<u32> TRANS_HTOTAL_WD0 {0x6E000};
	static constexpr BitRegister<u32> TRANS_VTOTAL_WD0 {0x6E00C};
	static constexpr BitRegister<u32> TRANS_HTOTAL_WD1 {0x6E800};
	static constexpr BitRegister<u32> TRANS_VTOTAL_WD1 {0x6E80C};
	static constexpr BitRegister<u32> TRANS_CONF_A {0x70008};
	static constexpr BitRegister<u32> PIPE_BOTTOM_COLOR_A {0x70034};
	static constexpr BitRegister<u32> PLANE_CTL_1_A {0x70180};
	static constexpr BitRegister<u32> PLANE_STRIDE_1_A {0x70188};
	static constexpr BitRegister<u32> PLANE_SIZE_1_A {0x70190};
	static constexpr BitRegister<u32> PLANE_SURF_1_A {0x7019C};
	static constexpr BitRegister<u32> PLANE_SURFLIVE_1_A {0x701AC};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_1_A {0x7027C};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_2_A {0x7037C};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_3_A {0x7047C};
	static constexpr BitRegister<u32> PLANE_CTL_2_A {0x70280};
	static constexpr BitRegister<u32> PLANE_SIZE_2_A {0x70290};
	static constexpr BitRegister<u32> PLANE_STRIDE_2_A {0x70288};
	static constexpr BitRegister<u32> PLANE_SURF_2_A {0x7029C};
	static constexpr BitRegister<u32> PLANE_CTL_3_A {0x70380};
	static constexpr BitRegister<u32> PLANE_SIZE_3_A {0x70390};
	static constexpr BitRegister<u32> PLANE_STRIDE_3_A {0x70388};
	static constexpr BitRegister<u32> PLANE_SURF_3_A {0x7039C};
	static constexpr BitRegister<u32> PLANE_CTL_4_A {0x70480};
	static constexpr BitRegister<u32> PLANE_SIZE_4_A {0x70490};
	static constexpr BitRegister<u32> PLANE_STRIDE_4_A {0x70488};
	static constexpr BitRegister<u32> PLANE_SURF_4_A {0x7049C};
	static constexpr BitRegister<u32> PLANE_CTL_5_A {0x70580};
	static constexpr BitRegister<u32> PLANE_SIZE_5_A {0x70590};
	static constexpr BitRegister<u32> PLANE_STRIDE_5_A {0x70588};
	static constexpr BitRegister<u32> PLANE_SURF_5_A {0x7059C};
	static constexpr BitRegister<u32> PLANE_CTL_6_A {0x70680};
	static constexpr BitRegister<u32> PLANE_SIZE_6_A {0x70690};
	static constexpr BitRegister<u32> PLANE_STRIDE_6_A {0x70688};
	static constexpr BitRegister<u32> PLANE_SURF_6_A {0x7069C};
	static constexpr BitRegister<u32> PLANE_CTL_7_A {0x70780};
	static constexpr BitRegister<u32> PLANE_SIZE_7_A {0x70790};
	static constexpr BitRegister<u32> PLANE_STRIDE_7_A {0x70788};
	static constexpr BitRegister<u32> PLANE_SURF_7_A {0x7089C};
	static constexpr BitRegister<u32> TRANS_CONF_B {0x71008};
	static constexpr BitRegister<u32> PIPE_BOTTOM_COLOR_B {0x71034};
	static constexpr BitRegister<u32> PLANE_CTL_1_B {0x71180};
	static constexpr BitRegister<u32> PLANE_STRIDE_1_B {0x71188};
	static constexpr BitRegister<u32> PLANE_SIZE_1_B {0x71190};
	static constexpr BitRegister<u32> PLANE_SURF_1_B {0x7119C};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_1_B {0x7127C};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_2_B {0x7137C};
	static constexpr BitRegister<u32> PLANE_BUF_CFG_3_B {0x7147C};
	static constexpr BitRegister<u32> PLANE_CTL_2_B {0x71280};
	static constexpr BitRegister<u32> PLANE_STRIDE_2_B {0x71288};
	static constexpr BitRegister<u32> PLANE_SIZE_2_B {0x71290};
	static constexpr BitRegister<u32> PLANE_SURF_2_B {0x7129C};
	static constexpr BitRegister<u32> PLANE_CTL_3_B {0x71380};
	static constexpr BitRegister<u32> PLANE_STRIDE_3_B {0x71388};
	static constexpr BitRegister<u32> PLANE_SIZE_3_B {0x71390};
	static constexpr BitRegister<u32> PLANE_SURF_3_B {0x7139C};
	static constexpr BitRegister<u32> PLANE_CTL_4_B {0x71480};
	static constexpr BitRegister<u32> PLANE_STRIDE_4_B {0x71488};
	static constexpr BitRegister<u32> PLANE_SIZE_4_B {0x71490};
	static constexpr BitRegister<u32> PLANE_SURF_4_B {0x7149C};
	static constexpr BitRegister<u32> PLANE_CTL_5_B {0x71580};
	static constexpr BitRegister<u32> PLANE_STRIDE_5_B {0x71588};
	static constexpr BitRegister<u32> PLANE_SIZE_5_B {0x71590};
	static constexpr BitRegister<u32> PLANE_SURF_5_B {0x7159C};
	static constexpr BitRegister<u32> PLANE_CTL_6_B {0x71680};
	static constexpr BitRegister<u32> PLANE_STRIDE_6_B {0x71688};
	static constexpr BitRegister<u32> PLANE_SIZE_6_B {0x71690};
	static constexpr BitRegister<u32> PLANE_SURF_6_B {0x7169C};
	static constexpr BitRegister<u32> PLANE_CTL_7_B {0x71780};
	static constexpr BitRegister<u32> PLANE_STRIDE_7_B {0x71788};
	static constexpr BitRegister<u32> PLANE_SIZE_7_B {0x71790};
	static constexpr BitRegister<u32> PLANE_SURF_7_B {0x7179C};
	static constexpr BitRegister<u32> TRANS_CONF_C {0x72008};
	static constexpr BitRegister<u32> PIPE_BOTTOM_COLOR_C {0x72034};
	static constexpr BitRegister<u32> TRANS_CONF_D {0x73008};
	static constexpr BitRegister<u32> PIPE_BOTTOM_COLOR_D {0x73034};
	static constexpr BitRegister<u32> PLANE_STRIDE_1_C {0x72188};
	static constexpr BitRegister<u32> PLANE_SIZE_1_C {0x72190};
	static constexpr BitRegister<u32> PLANE_SURF_1_C {0x7219C};
	static constexpr BitRegister<u32> PLANE_STRIDE_2_C {0x72288};
	static constexpr BitRegister<u32> PLANE_SIZE_2_C {0x72290};
	static constexpr BitRegister<u32> PLANE_SURF_2_C {0x7229C};
	static constexpr BitRegister<u32> PLANE_STRIDE_3_C {0x72388};
	static constexpr BitRegister<u32> PLANE_SIZE_3_C {0x72390};
	static constexpr BitRegister<u32> PLANE_SURF_3_C {0x7239C};
	static constexpr BitRegister<u32> PLANE_STRIDE_4_C {0x72488};
	static constexpr BitRegister<u32> PLANE_SIZE_4_C {0x72490};
	static constexpr BitRegister<u32> PLANE_SURF_4_C {0x7249C};
	static constexpr BitRegister<u32> PLANE_STRIDE_5_C {0x72588};
	static constexpr BitRegister<u32> PLANE_SIZE_5_C {0x72590};
	static constexpr BitRegister<u32> PLANE_SURF_5_C {0x7259C};
	static constexpr BitRegister<u32> PLANE_STRIDE_6_C {0x72688};
	static constexpr BitRegister<u32> PLANE_SIZE_6_C {0x72690};
	static constexpr BitRegister<u32> PLANE_SURF_6_C {0x7269C};
	static constexpr BitRegister<u32> PLANE_STRIDE_7_C {0x72788};
	static constexpr BitRegister<u32> PLANE_SIZE_7_C {0x72790};
	static constexpr BitRegister<u32> PLANE_SURF_7_C {0x7279C};
	static constexpr BitRegister<u32> PLANE_STRIDE_1_D {0x73188};
	static constexpr BitRegister<u32> PLANE_SIZE_1_D {0x73190};
	static constexpr BitRegister<u32> PLANE_SURF_1_D {0x7319C};
	static constexpr BitRegister<u32> PLANE_STRIDE_2_D {0x73288};
	static constexpr BitRegister<u32> PLANE_SIZE_2_D {0x73290};
	static constexpr BitRegister<u32> PLANE_SURF_2_D {0x7329C};
	static constexpr BitRegister<u32> PLANE_STRIDE_3_D {0x73388};
	static constexpr BitRegister<u32> PLANE_SIZE_3_D {0x73390};
	static constexpr BitRegister<u32> PLANE_SURF_3_D {0x7339C};
	static constexpr BitRegister<u32> PLANE_STRIDE_4_D {0x73488};
	static constexpr BitRegister<u32> PLANE_SIZE_4_D {0x73490};
	static constexpr BitRegister<u32> PLANE_SURF_4_D {0x7349C};
	static constexpr BitRegister<u32> PLANE_STRIDE_5_D {0x73588};
	static constexpr BitRegister<u32> PLANE_SIZE_5_D {0x73590};
	static constexpr BitRegister<u32> PLANE_SURF_5_D {0x7359C};
	static constexpr BitRegister<u32> PLANE_STRIDE_6_D {0x73688};
	static constexpr BitRegister<u32> PLANE_SIZE_6_D {0x73690};
	static constexpr BitRegister<u32> PLANE_SURF_6_D {0x7369C};
	static constexpr BitRegister<u32> PLANE_STRIDE_7_D {0x73788};
	static constexpr BitRegister<u32> PLANE_SIZE_7_D {0x73790};
	static constexpr BitRegister<u32> PLANE_SURF_7_D {0x7379C};
	static constexpr BitRegister<u32> SFUSE_STRAP {0xC2014};
	static constexpr BitRegister<u32> SDEISR {0xC4000};
	static constexpr BitRegister<u32> SDEIMR {0xC4004};
	static constexpr BitRegister<u32> SDEIIR {0xC4008};
	static constexpr BitRegister<u32> SDEIER {0xC400C};
	static constexpr BitRegister<u32> SHOTPLUG_CTL {0xC4030};
	static constexpr BitRegister<u32> GMBUS0 {0xC5100};
	static constexpr BitRegister<u32> GMBUS1 {0xC5104};
	static constexpr BitRegister<u32> GMBUS2 {0xC5108};
	static constexpr BasicRegister<u32> GMBUS3 {0xC510C};
	static constexpr BitRegister<u32> GMBUS4 {0xC5110};
	static constexpr BitRegister<u32> GMBUS5 {0xC5120};
	static constexpr BitRegister<u32> GTDRIVER_MAILBOX_INTERFACE {0x138124};
	static constexpr BasicRegister<u32> GTDRIVER_MAILBOX_DATA0 {0x138128};
	static constexpr BasicRegister<u32> GTDRIVER_MAILBOX_DATA1 {0x13812C};
	static constexpr BitRegister<u32> PORT_CL_DW5_C {0x160014};
	static constexpr BitRegister<u32> PORT_CL_DW10_C {0x160028};
	static constexpr BitRegister<u32> PORT_COMP_DW0_C {0x160100};
	static constexpr BitRegister<u32> PORT_COMP_DW1_C {0x160104};
	static constexpr BitRegister<u32> PORT_COMP_DW3_C {0x16010C};
	static constexpr BitRegister<u32> PORT_COMP_DW9_C {0x160124};
	static constexpr BitRegister<u32> PORT_COMP_DW10_C {0x160128};
	static constexpr BitRegister<u32> PORT_PCS_DW1_AUX_C {0x160304};
	static constexpr BitRegister<u32> PORT_TX_DW4_AUX_C {0x160390};
	static constexpr BitRegister<u32> PORT_TX_DW8_AUX_C {0x1603A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_GRP_C {0x160604};
	static constexpr BitRegister<u32> PORT_TX_DW4_GRP_C {0x160690};
	static constexpr BitRegister<u32> PORT_TX_DW8_GRP_C {0x1606A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_LN0_C {0x160804};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN0_C {0x160890};
	static constexpr BitRegister<u32> PORT_TX_DW8_LN0_C {0x1608A0};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN1_C {0x160990};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN2_C {0x160A90};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN3_C {0x160B90};
	static constexpr BitRegister<u32> PORT_CL_DW5_A {0x162014};
	static constexpr BitRegister<u32> PORT_CL_DW10_A {0x162028};
	static constexpr BitRegister<u32> PORT_COMP_DW0_A {0x162100};
	static constexpr BitRegister<u32> PORT_COMP_DW1_A {0x162104};
	static constexpr BitRegister<u32> PORT_COMP_DW3_A {0x16210C};
	static constexpr BitRegister<u32> PORT_COMP_DW8_A {0x162120};
	static constexpr BitRegister<u32> PORT_COMP_DW9_A {0x162124};
	static constexpr BitRegister<u32> PORT_COMP_DW10_A {0x162128};
	static constexpr BitRegister<u32> PORT_PCS_DW1_AUX_A {0x162304};
	static constexpr BitRegister<u32> PORT_TX_DW4_AUX_A {0x162390};
	static constexpr BitRegister<u32> PORT_TX_DW5_AUX_A {0x162394};
	static constexpr BitRegister<u32> PORT_TX_DW8_AUX_A {0x1623A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_GRP_A {0x162604};
	static constexpr BitRegister<u32> PORT_TX_DW4_GRP_A {0x162690};
	static constexpr BitRegister<u32> PORT_TX_DW5_GRP_A {0x162694};
	static constexpr BitRegister<u32> PORT_TX_DW8_GRP_A {0x1626A0};
	static constexpr BitRegister<u32> PORT_PCS_DW1_LN0_A {0x162804};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN0_A {0x162890};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN0_A {0x162894};
	static constexpr BitRegister<u32> PORT_TX_DW8_LN0_A {0x1628A0};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN1_A {0x162990};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN1_A {0x162994};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN2_A {0x162A90};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN2_A {0x162A94};
	static constexpr BitRegister<u32> PORT_TX_DW4_LN3_A {0x162B90};
	static constexpr BitRegister<u32> PORT_TX_DW5_LN3_A {0x162B94};
	static constexpr BitRegister<u32> DPCLKA_CFGCR0 {0x164280};
	static constexpr BitRegister<u32> DPLL0_CFGCR0 {0x164284};
	static constexpr BitRegister<u32> DPLL0_CFGCR1 {0x164288};
	static constexpr BitRegister<u32> DPLL1_CFGCR0 {0x16428C};
	static constexpr BitRegister<u32> DPLL1_CFGCR1 {0x164290};
	static constexpr BitRegister<u32> DPLL4_CFGCR0 {0x164294};
	static constexpr BitRegister<u32> DPLL4_CFGCR1 {0x164298};
	static constexpr BitRegister<u32> TBTPLL_CFGCR0 {0x16429C};
	static constexpr BitRegister<u32> TBTPLL_CFGCR1 {0x1642A0};
	static constexpr BitRegister<u32> DPLL0_SSC {0x164B10};
	static constexpr BitRegister<u32> DPLL1_SSC {0x164C10};
	static constexpr BitRegister<u32> DPLL4_SSC {0x164E10};
	static constexpr BitRegister<u32> GFX_MSTR_INTR {0x190010};

	static constexpr BitRegister<u32> PORT_COMP_DW0[] {
		PORT_COMP_DW0_A,
		PORT_COMP_DW0_B,
		PORT_COMP_DW0_C
	};

	static constexpr BitRegister<u32> PORT_TX_DW8_AUX[] {
		PORT_TX_DW8_AUX_A,
		PORT_TX_DW8_AUX_B,
		PORT_TX_DW8_AUX_C
	};

	static constexpr BitRegister<u32> PORT_TX_DW8_GRP[] {
		PORT_TX_DW8_GRP_A,
		PORT_TX_DW8_GRP_B,
		PORT_TX_DW8_GRP_C
	};

	static constexpr BitRegister<u32> PORT_TX_DW8_LN0[] {
		PORT_TX_DW8_LN0_A,
		PORT_TX_DW8_LN0_B,
		PORT_TX_DW8_LN0_C
	};

	static constexpr BitRegister<u32> PORT_PCS_DW1_AUX[] {
		PORT_PCS_DW1_AUX_A,
		PORT_PCS_DW1_AUX_B,
		PORT_PCS_DW1_AUX_C
	};

	static constexpr BitRegister<u32> PORT_PCS_DW1_GRP[] {
		PORT_PCS_DW1_GRP_A,
		PORT_PCS_DW1_GRP_B,
		PORT_PCS_DW1_GRP_C
	};

	static constexpr BitRegister<u32> PORT_PCS_DW1_LN0[] {
		PORT_PCS_DW1_LN0_A,
		PORT_PCS_DW1_LN0_B,
		PORT_PCS_DW1_LN0_C
	};

	static constexpr BitRegister<u32> PHY_MISC[] {
		PHY_MISC_A,
		PHY_MISC_B,
		PHY_MISC_C
	};

	static constexpr BitRegister<u32> PORT_COMP_DW1[] {
		PORT_COMP_DW1_A,
		PORT_COMP_DW1_B,
		PORT_COMP_DW1_C
	};

	static constexpr BitRegister<u32> PORT_COMP_DW3[] {
		PORT_COMP_DW3_A,
		PORT_COMP_DW3_B,
		PORT_COMP_DW3_C
	};

	static constexpr BitRegister<u32> PORT_COMP_DW9[] {
		PORT_COMP_DW9_A,
		PORT_COMP_DW9_B,
		PORT_COMP_DW9_C
	};

	static constexpr BitRegister<u32> PORT_COMP_DW10[] {
		PORT_COMP_DW10_A,
		PORT_COMP_DW10_B,
		PORT_COMP_DW10_C
	};

	static constexpr BitRegister<u32> PORT_CL_DW5[] {
		PORT_CL_DW5_A,
		PORT_CL_DW5_B,
		PORT_CL_DW5_C
	};

	static constexpr BitRegister<u32> DDI_AUX_CTL[] {
		DDI_AUX_CTL_A,
		DDI_AUX_CTL_B,
		DDI_AUX_CTL_C,
		DDI_AUX_CTL_USBC1,
		DDI_AUX_CTL_USBC2,
		DDI_AUX_CTL_USBC3,
		DDI_AUX_CTL_USBC4,
		DDI_AUX_CTL_USBC5,
		DDI_AUX_CTL_USBC6
	};

	static constexpr BasicRegister<u32> DDI_AUX_DATA0[] {
		DDI_AUX_DATA_0_A,
		DDI_AUX_DATA_0_B,
		DDI_AUX_DATA_0_C,
		DDI_AUX_DATA_0_USBC1,
		DDI_AUX_DATA_0_USBC2,
		DDI_AUX_DATA_0_USBC3,
		DDI_AUX_DATA_0_USBC4,
		DDI_AUX_DATA_0_USBC5,
		DDI_AUX_DATA_0_USBC6
	};
	static constexpr BasicRegister<u32> DDI_AUX_DATA1[] {
		DDI_AUX_DATA_1_A,
		DDI_AUX_DATA_1_B,
		DDI_AUX_DATA_1_C,
		DDI_AUX_DATA_1_USBC1,
		DDI_AUX_DATA_1_USBC2,
		DDI_AUX_DATA_1_USBC3,
		DDI_AUX_DATA_1_USBC4,
		DDI_AUX_DATA_1_USBC5,
		DDI_AUX_DATA_1_USBC6
	};
	static constexpr BasicRegister<u32> DDI_AUX_DATA2[] {
		DDI_AUX_DATA_2_A,
		DDI_AUX_DATA_2_B,
		DDI_AUX_DATA_2_C,
		DDI_AUX_DATA_2_USBC1,
		DDI_AUX_DATA_2_USBC2,
		DDI_AUX_DATA_2_USBC3,
		DDI_AUX_DATA_2_USBC4,
		DDI_AUX_DATA_2_USBC5,
		DDI_AUX_DATA_2_USBC6
	};
	static constexpr BasicRegister<u32> DDI_AUX_DATA3[] {
		DDI_AUX_DATA_3_A,
		DDI_AUX_DATA_3_B,
		DDI_AUX_DATA_3_C,
		DDI_AUX_DATA_3_USBC1,
		DDI_AUX_DATA_3_USBC2,
		DDI_AUX_DATA_3_USBC3,
		DDI_AUX_DATA_3_USBC4,
		DDI_AUX_DATA_3_USBC5,
		DDI_AUX_DATA_3_USBC6
	};
	static constexpr BasicRegister<u32> DDI_AUX_DATA4[] {
		DDI_AUX_DATA_4_A,
		DDI_AUX_DATA_4_B,
		DDI_AUX_DATA_4_C,
		DDI_AUX_DATA_4_USBC1,
		DDI_AUX_DATA_4_USBC2,
		DDI_AUX_DATA_4_USBC3,
		DDI_AUX_DATA_4_USBC4,
		DDI_AUX_DATA_4_USBC5,
		DDI_AUX_DATA_4_USBC6
	};
}

namespace nde_rstwrn_opt {
	static constexpr BitField<u32, bool> RST_PCH_HANDSHAKE_EN {4, 1};
}

namespace port_comp_dw0 {
	static constexpr BitField<u32, bool> COMP_INIT {31, 1};
}

namespace port_tx_dw8 {
	static constexpr BitField<u32, bool> ODCC_CLK_DIV_SEL {29, 2};
	static constexpr BitField<u32, u8> ODCC_CLKSEL {31, 1};

	static constexpr u8 CLK_DIV_2 = 0;
}

namespace port_pcs_dw1 {
	static constexpr BitField<u32, u8> DCC_MODE_SELECT {20, 2};
	static constexpr BitField<u32, bool> CMN_KEEPER_ENABLE {26, 1};

	static constexpr u8 DCC_ONCE = 0;
	static constexpr u8 DCC_CONTINUOUS = 0b11;
}

namespace phy_misc {
	static constexpr BitField<u32, bool> DE_TO_IO_COMP_PWR_DOWN {23, 1};
}

namespace port_comp_dw3 {
	static constexpr BitField<u32, u8> VOLTAGE {24, 2};
	static constexpr BitField<u32, u8> PROCESS {26, 3};

	static constexpr u8 DOT0 = 0;
	static constexpr u8 DOT1 = 1;
	static constexpr u8 DOT4 = 0b10;

	static constexpr u8 VOLT_0_85 = 0;
	static constexpr u8 VOLT_0_95 = 1;
	static constexpr u8 VOLT_1_05 = 0b10;
}

namespace port_comp_dw8 {
	static constexpr BitField<u32, bool> IREFGEN {24, 1};
}

namespace port_cl_dw5 {
	static constexpr BitField<u32, u8> SUS_CLOCK_CONFIG {0, 2};
	static constexpr BitField<u32, bool> CL_POWER_DOWN_ENABLE {4, 1};
}

namespace fuse_status {
	static constexpr BitField<u32, bool> FUSE_PG5_DIST_STATUS {22, 1};
	static constexpr BitField<u32, bool> FUSE_PG4_DIST_STATUS {23, 1};
	static constexpr BitField<u32, bool> FUSE_PG3_DIST_STATUS {24, 1};
	static constexpr BitField<u32, bool> FUSE_PG2_DIST_STATUS {25, 1};
	static constexpr BitField<u32, bool> FUSE_PG1_DIST_STATUS {26, 1};
	static constexpr BitField<u32, bool> FUSE_PG0_DIST_STATUS {27, 1};
}

namespace pwr_well_ctl {
	static constexpr BitField<u32, bool> POWER_WELL_1_STATE {0, 1};
	static constexpr BitField<u32, bool> POWER_WELL_1_REQ {1, 1};
	static constexpr BitField<u32, bool> POWER_WELL_2_STATE {2, 1};
	static constexpr BitField<u32, bool> POWER_WELL_2_REQ {3, 1};
	static constexpr BitField<u32, bool> POWER_WELL_3_STATE {4, 1};
	static constexpr BitField<u32, bool> POWER_WELL_3_REQ {5, 1};
	static constexpr BitField<u32, bool> POWER_WELL_4_STATE {6, 1};
	static constexpr BitField<u32, bool> POWER_WELL_4_REQ {7, 1};
	static constexpr BitField<u32, bool> POWER_WELL_5_STATE {8, 1};
	static constexpr BitField<u32, bool> POWER_WELL_5_REQ {9, 1};
}

namespace dssm {
	static constexpr BitField<u32, bool> DISPLAYPORT_A_PRESENT {0, 1};
	static constexpr BitField<u32, u8> REF_FREQ {29, 3};

	static constexpr u8 FREQ_24 = 0;
	static constexpr u8 FREQ_19_2 = 1;
	static constexpr u8 FREQ_38_4 = 0b10;
}

namespace pwr_well_ctl_aux {
	static constexpr BitField<u32, bool> AUX_A_IO_POWER_STATE {0, 1};
	static constexpr BitField<u32, bool> AUX_A_IO_POWER_REQ {1, 1};
	static constexpr BitField<u32, bool> AUX_B_IO_POWER_STATE {2, 1};
	static constexpr BitField<u32, bool> AUX_B_IO_POWER_REQ {3, 1};
	static constexpr BitField<u32, bool> AUX_C_IO_POWER_STATE {4, 1};
	static constexpr BitField<u32, bool> AUX_C_IO_POWER_REQ {5, 1};
	static constexpr BitField<u32, bool> AUX_USBC1_IO_POWER_STATE {6, 1};
	static constexpr BitField<u32, bool> AUX_USBC1_IO_POWER_REQ {7, 1};
	static constexpr BitField<u32, bool> AUX_USBC2_IO_POWER_STATE {8, 1};
	static constexpr BitField<u32, bool> AUX_USBC2_IO_POWER_REQ {9, 1};
	static constexpr BitField<u32, bool> AUX_USBC3_IO_POWER_STATE {10, 1};
	static constexpr BitField<u32, bool> AUX_USBC3_IO_POWER_REQ {11, 1};
	static constexpr BitField<u32, bool> AUX_USBC4_IO_POWER_STATE {12, 1};
	static constexpr BitField<u32, bool> AUX_USBC4_IO_POWER_REQ {13, 1};
	static constexpr BitField<u32, bool> AUX_USBC5_IO_POWER_STATE {14, 1};
	static constexpr BitField<u32, bool> AUX_USBC5_IO_POWER_REQ {15, 1};
	static constexpr BitField<u32, bool> AUX_USBC6_IO_POWER_STATE {16, 1};
	static constexpr BitField<u32, bool> AUX_USBC6_IO_POWER_REQ {17, 1};
	static constexpr BitField<u32, bool> AUX_D_IO_POWER_STATE {14, 1};
	static constexpr BitField<u32, bool> AUX_D_IO_POWER_REQ {15, 1};
}

namespace gt_driver_mailbox {
	static constexpr BitField<u32, bool> BUSY {31, 1};
}

namespace cdclk_pll_enable {
	static constexpr BitField<u32, u8> PLL_RATIO {0, 8};
	static constexpr BitField<u32, bool> PLL_LOCK {30, 1};
	static constexpr BitField<u32, bool> PLL_ENABLE {31, 1};
}

namespace cdclk_ctl {
	static constexpr BitField<u32, u16> CD_FREQ_DECIMAL {0, 11};
	static constexpr BitField<u32, u8> CD2X_DIVIDER {22, 2};

	static constexpr u8 DIV_BY_1 = 0;
	static constexpr u8 DIV_BY_2 = 0b10;

	static constexpr u8 FREQ_192 = 0b0010111111;
}

namespace dbuf_ctl {
	static constexpr BitField<u32, u8> TRACKER_STATE_SERVICE {19, 5};
	static constexpr BitField<u32, bool> POWER_STATE {30, 1};
	static constexpr BitField<u32, bool> POWER_REQ {31, 1};
}

namespace mbus_abox_ctl {
	static constexpr BitField<u32, u8> BT_CREDITS_POOL1 {0, 5};
	static constexpr BitField<u32, u8> BT_CREDITS_POOL2 {8, 5};
	static constexpr BitField<u32, u8> B_CREDITS {16, 4};
	static constexpr BitField<u32, u8> BW_CREDITS {20, 2};
}

namespace bw_buddy_ctl {
	static constexpr BitField<u32, bool> BUDDY_DISABLE {31, 1};
}

namespace shotplug_ctl {
	static constexpr BitField<u32, u8> DDI_A_HPD_STATUS {0, 2};
	static constexpr BitField<u32, u8> DDI_A_HPD_OUTPUT_DATA {2, 1};
	static constexpr BitField<u32, bool> DDI_A_HPD_ENABLE {3, 1};
	static constexpr BitField<u32, u8> DDI_B_HPD_STATUS {4, 2};
	static constexpr BitField<u32, u8> DDI_B_HPD_OUTPUT_DATA {6, 1};
	static constexpr BitField<u32, bool> DDI_B_HPD_ENABLE {7, 1};
	static constexpr BitField<u32, u8> DDI_C_HPD_STATUS {8, 2};
	static constexpr BitField<u32, u8> DDI_C_HPD_OUTPUT_DATA {10, 1};
	static constexpr BitField<u32, bool> DDI_C_HPD_ENABLE {11, 1};
	static constexpr BitField<u32, u8> DDI_D_HPD_STATUS {12, 2};
	static constexpr BitField<u32, u8> DDI_D_HPD_OUTPUT_DATA {14, 1};
	static constexpr BitField<u32, bool> DDI_D_HPD_ENABLE {15, 1};
}

namespace sfuse_strap {
	static constexpr BitField<u32, bool> PORT_D_PRESENT {0, 1};
	static constexpr BitField<u32, bool> PORT_C_PRESENT {1, 1};
	static constexpr BitField<u32, bool> PORT_B_PRESENT {2, 1};
}

namespace display_int_ctl {
	static constexpr BitField<u32, bool> DE_PCH_INTERRUPTS_PENDING {23, 1};
	static constexpr BitField<u32, bool> ENABLE {31, 1};
}

namespace dpll_enable {
	static constexpr BitField<u32, bool> POWER_STATE {26, 1};
	static constexpr BitField<u32, bool> POWER_ENABLE {27, 1};
	static constexpr BitField<u32, bool> PLL_LOCK {30, 1};
	static constexpr BitField<u32, bool> PLL_ENABLE {31, 1};
}

namespace sinterrupt {
	static constexpr BitField<u32, bool> DDI_A_HOTPLUG {16, 1};
	static constexpr BitField<u32, bool> DDI_B_HOTPLUG {17, 1};
	static constexpr BitField<u32, bool> DDI_C_HOTPLUG {18, 1};
	static constexpr BitField<u32, bool> DDI_D_HOTPLUG {19, 1};
}

namespace ddi_aux_ctl {
	static constexpr BitField<u32, u8> SYNC_PULSE_CNT {0, 5};
	static constexpr BitField<u32, u8> FAST_WAKE_SYNC_PULSE_CNT {5, 5};
	static constexpr BitField<u32, u8> IO_SELECT {11, 1};
	static constexpr BitField<u32, u8> MESSAGE_SIZE {20, 5};
	static constexpr BitField<u32, bool> RECEIVE_ERROR {25, 1};
	static constexpr BitField<u32, u8> TIMEOUT_VALUE {26, 2};
	static constexpr BitField<u32, bool> TIMEOUT_ERROR {28, 1};
	static constexpr BitField<u32, bool> INTERRUPT_ON_DONE {29, 1};
	static constexpr BitField<u32, bool> DONE {30, 1};
	static constexpr BitField<u32, bool> SEND_BUSY {31, 1};

	static constexpr u8 USE_TBT_IO = 1;
	static constexpr u8 USE_LEGACY_IO = 0;

	static constexpr u8 DEF_WAKE_SYNC_PULSE_CNT = 7;
	static constexpr u8 DEF_SYNC_PULSE_CNT = 32;
	static constexpr u8 TIMEOUT_4000US = 0b11;
}

namespace pwr_well_ctl_ddi {
	static constexpr BitField<u32, bool> DDI_A_IO_POWER_STATE {0, 1};
	static constexpr BitField<u32, bool> DDI_A_IO_POWER_REQ {1, 1};
	static constexpr BitField<u32, bool> DDI_B_IO_POWER_STATE {2, 1};
	static constexpr BitField<u32, bool> DDI_B_IO_POWER_REQ {3, 1};
	static constexpr BitField<u32, bool> DDI_C_IO_POWER_STATE {4, 1};
	static constexpr BitField<u32, bool> DDI_C_IO_POWER_REQ {5, 1};
}

namespace tc_hotplug_ctl {
	static constexpr BitField<u32, u8> PORT1_HPD_STATUS {0, 2};
	static constexpr BitField<u32, bool> PORT1_HPD_ENABLE {3, 1};
	static constexpr BitField<u32, u8> PORT2_HPD_STATUS {4, 2};
	static constexpr BitField<u32, bool> PORT2_HPD_ENABLE {7, 1};
	static constexpr BitField<u32, u8> PORT3_HPD_STATUS {8, 2};
	static constexpr BitField<u32, bool> PORT3_HPD_ENABLE {11, 1};
	static constexpr BitField<u32, u8> PORT4_HPD_STATUS {12, 2};
	static constexpr BitField<u32, bool> PORT4_HPD_ENABLE {15, 1};
	static constexpr BitField<u32, u8> PORT5_HPD_STATUS {16, 2};
	static constexpr BitField<u32, bool> PORT5_HPD_ENABLE {19, 1};
	static constexpr BitField<u32, u8> PORT6_HPD_STATUS {20, 2};
	static constexpr BitField<u32, bool> PORT6_HPD_ENABLE {23, 1};
	static constexpr BitField<u32, u8> PORT7_HPD_STATUS {24, 2};
	static constexpr BitField<u32, bool> PORT7_HPD_ENABLE {27, 1};
	static constexpr BitField<u32, u8> PORT8_HPD_STATUS {28, 2};
	static constexpr BitField<u32, bool> PORT8_HPD_ENABLE {31, 1};
}

namespace de_pipe_int {
	static constexpr BitField<u32, bool> VBLANK {0, 1};
	static constexpr BitField<u32, bool> VSYNC {1, 1};
	static constexpr BitField<u32, bool> SCANLINE_EVENT {2, 1};
	static constexpr BitField<u32, bool> PLANE1_FLIP_DONE {3, 1};
	static constexpr BitField<u32, bool> PLANE2_FLIP_DONE {4, 1};
	static constexpr BitField<u32, bool> PLANE3_FLIP_DONE {5, 1};
	static constexpr BitField<u32, bool> PLANE4_FLIP_DONE {6, 1};
	static constexpr BitField<u32, bool> PLANE1_GTT_FAULT {7, 1};
	static constexpr BitField<u32, bool> PLANE2_GTT_FAULT {8, 1};
	static constexpr BitField<u32, bool> PLANE3_GTT_FAULT {9, 1};
	static constexpr BitField<u32, bool> PLANE4_GTT_FAULT {10, 1};
	static constexpr BitField<u32, bool> CURSOR_GTT_FAULT {11, 1};
	static constexpr BitField<u32, bool> DPST_HISTOGRAM_EVENT {12, 1};
	static constexpr BitField<u32, bool> DSB0_INTERRUPT {13, 1};
	static constexpr BitField<u32, bool> DSB1_INTERRUPT {14, 1};
	static constexpr BitField<u32, bool> DSB2_INTERRUPT {15, 1};
	static constexpr BitField<u32, bool> PLANE5_FLIP_DONE {16, 1};
	static constexpr BitField<u32, bool> PLANE6_FLIP_DONE {17, 1};
	static constexpr BitField<u32, bool> PLANE7_FLIP_DONE {18, 1};
	static constexpr BitField<u32, bool> VBLANK_UNMODIFIED {19, 1};
	static constexpr BitField<u32, bool> PLANE5_GTT_FAULT {20, 1};
	static constexpr BitField<u32, bool> PLANE6_GTT_FAULT {21, 1};
	static constexpr BitField<u32, bool> PLANE7_GTT_FAULT {22, 1};
	static constexpr BitField<u32, bool> LACE_FAST_ACCESS_INTERRUPT {23, 1};
	static constexpr BitField<u32, bool> PIPE_DMC_GTT_FAULT {25, 1};
	static constexpr BitField<u32, bool> PIPE_DMC_INTERRUPT {26, 1};
	static constexpr BitField<u32, bool> ISOCH_ACK {27, 1};
	static constexpr BitField<u32, bool> VRR_DOUBLE_BUFFER_UPDATE {30, 1};
	static constexpr BitField<u32, bool> UNDERRUN {31, 1};
}

namespace gfx_mstr_intr {
	static constexpr BitField<u32, bool> GT_DW0 {0, 1};
	static constexpr BitField<u32, bool> GT_DW1 {1, 1};
	static constexpr BitField<u32, bool> DISPLAY {16, 1};
	static constexpr BitField<u32, bool> GU_MISC {29, 1};
	static constexpr BitField<u32, bool> PCU {30, 1};
	static constexpr BitField<u32, bool> PRIMARY_INT {31, 1};
}

namespace dpll_ssc {
	static constexpr BitField<u32, u8> INIT_DCOAMP {0, 6};
	static constexpr BitField<u32, u8> BIAS_GB_SEL {6, 2};
	static constexpr BitField<u32, bool> SSC_FLL_EN {8, 1};
	static constexpr BitField<u32, bool> SSC_EN {9, 1};
	static constexpr BitField<u32, bool> SSC_OPENLOOP_EN {10, 1};
	static constexpr BitField<u32, u8> SSC_STEP_NUM {11, 3};
	static constexpr BitField<u32, u8> SSC_FLL_UPDATE_SEL {14, 2};
	static constexpr BitField<u32, u8> SSC_STEP_LENGTH {16, 8};
	static constexpr BitField<u32, bool> SSC_INJ_EN {24, 1};
	static constexpr BitField<u32, bool> SSC_INJ_ADAPT_EN {25, 1};
	static constexpr BitField<u32, u8> SSC_STEP_NUM_OFFSET {26, 3};
	static constexpr BitField<u32, u8> IREF_NDIV_RATIO {29, 3};
}

namespace dpll_cfgcr0 {
	static constexpr BitField<u32, u16> DCO_INTEGER {0, 10};
	static constexpr BitField<u32, u16> DCO_FRACTION {10, 15};
}

namespace dpll_cfgcr1 {
	static constexpr BitField<u32, u8> PDIV {2, 4};
	static constexpr BitField<u32, u8> KDIV {6, 3};
	static constexpr BitField<u32, bool> QDIV_MODE {9, 1};
	static constexpr BitField<u32, u8> QDIV_RATIO {10, 8};

	static constexpr u8 PDIV_2 = 1;
	static constexpr u8 PDIV_3 = 0b10;
	static constexpr u8 PDIV_5 = 0b100;
	static constexpr u8 PDIV_7 = 0b1000;

	static constexpr u8 KDIV_1 = 1;
	static constexpr u8 KDIV_2 = 0b10;
	static constexpr u8 KDIV_3 = 0b100;
}

namespace dpclka_cfgcr0 {
	static constexpr BitField<u32, u8> DDIA_CLOCK_SELECT {0, 2};
	static constexpr BitField<u32, u8> DDIB_CLOCK_SELECT {2, 2};
	static constexpr BitField<u32, u8> DDIC_CLOCK_SELECT {4, 2};
	static constexpr BitField<u32, bool> DDIA_CLOCK_OFF {10, 1};
	static constexpr BitField<u32, bool> DDIB_CLOCK_OFF {11, 1};
	static constexpr BitField<u32, bool> TC1_CLOCK_OFF {12, 1};
	static constexpr BitField<u32, bool> TC2_CLOCK_OFF {13, 1};
	static constexpr BitField<u32, bool> TC3_CLOCK_OFF {14, 1};
	static constexpr BitField<u32, bool> TC4_CLOCK_OFF {21, 1};
	static constexpr BitField<u32, bool> TC5_CLOCK_OFF {22, 1};
	static constexpr BitField<u32, bool> TC6_CLOCK_OFF {23, 1};
	static constexpr BitField<u32, bool> DDIC_CLOCK_OFF {24, 1};

	static constexpr u8 DPLL0 = 0;
	static constexpr u8 DPLL1 = 1;
	static constexpr u8 DPLL4 = 0b10;
}

namespace trans_clk_sel {
	static constexpr BitField<u32, u8> TRANS_CLOCK_SELECT {28, 4};

	static constexpr u8 NONE = 0;
	static constexpr u8 DDIA = 1;
	static constexpr u8 DDIB = 0b10;
	static constexpr u8 DDIC = 0b11;
	static constexpr u8 DDI_USBC1 = 0b100;
	static constexpr u8 DDI_USBC2 = 0b101;
	static constexpr u8 DDI_USBC3 = 0b110;
	static constexpr u8 DDI_USBC4 = 0b111;
	static constexpr u8 DDI_USBC5 = 0b1000;
	static constexpr u8 DDI_USBC6 = 0b1001;
}

namespace trans_ddi_func_ctl {
	static constexpr BitField<u32, bool> HDMI_SCRAMBLING_ENABLE {0, 1};
	static constexpr BitField<u32, u8> PORT_WIDTH_SELECT {1, 3};
	static constexpr BitField<u32, bool> HIGH_TMDS_CHAR_RATE {4, 1};
	static constexpr BitField<u32, u8> HDMI_SCRAMBLER_RESET_FREQ {6, 1};
	static constexpr BitField<u32, bool> HDMI_SCRAMBLER_CTS_ENABLE {7, 1};
	static constexpr BitField<u32, bool> DP_VC_PAYLOAD_ALLOCATE {8, 1};
	static constexpr BitField<u32, u8> MST_TRANSPORT_SELECT {10, 2};
	static constexpr BitField<u32, u8> DSI_INPUT_SELECT {12, 3};
	static constexpr BitField<u32, u8> SYNC_POLARITY {16, 2};
	static constexpr BitField<u32, u8> BITS_PER_COLOR {10, 3};
	static constexpr BitField<u32, u8> TRANS_DDI_MODE_SELECT {24, 3};
	static constexpr BitField<u32, u8> DDI_SELECT {27, 4};
	static constexpr BitField<u32, bool> TRANS_DDI_FUNC_ENABLE {31, 1};

	static constexpr u8 NONE = 0;
	static constexpr u8 DDIA = 1;
	static constexpr u8 DDIB = 0b10;
	static constexpr u8 DDIC = 0b11;
	static constexpr u8 DDI_USBC1 = 0b100;
	static constexpr u8 DDI_USBC2 = 0b101;
	static constexpr u8 DDI_USBC3 = 0b110;
	static constexpr u8 DDI_USBC4 = 0b111;
	static constexpr u8 DDI_USBC5 = 0b1000;
	static constexpr u8 DDI_USBC6 = 0b1001;

	static constexpr u8 MODE_HDMI = 0;
	static constexpr u8 MODE_DVI = 1;
	static constexpr u8 MODE_DP_SST = 0b10;
	static constexpr u8 MODE_DP_MST = 0b11;

	static constexpr u8 BPC8 = 0;
	static constexpr u8 BPC10 = 1;
	static constexpr u8 BPC6 = 0b10;
	static constexpr u8 BPC12 = 0b11;

	static constexpr u8 SYNC_LOW = 0;
	static constexpr u8 SYNC_VS_LOW_HS_HIGH = 1;
	static constexpr u8 SYNC_VS_HIGH_HS_LOW = 0b10;
	static constexpr u8 SYNC_HIGH = 0b11;

	static constexpr u8 DSI_INPUT_PIPEA = 0;
	static constexpr u8 DSI_INPUT_PIPEB = 0b101;
	static constexpr u8 DSI_INPUT_PIPEC = 0b110;
	static constexpr u8 DSI_INPUT_PIPED = 0b111;

	static constexpr u8 WIDTH_1 = 0;
	static constexpr u8 WIDTH_2 = 1;
	static constexpr u8 WIDTH_3 = 0b10;
	static constexpr u8 WIDTH_4 = 0b11;

	static constexpr u8 RESET_EVERY_LINE = 0;
	static constexpr u8 RESET_EVERY_OTHER_LINE = 1;

	static constexpr u8 MST_DPTP_A = 0;
	static constexpr u8 MST_DPTP_B = 1;
	static constexpr u8 MST_DPTP_C = 0b10;
	static constexpr u8 MST_DPTP_D = 0b11;
}

namespace dp_tp_ctl {
	static constexpr BitField<u32, bool> ALTERNATE_SR_ENABLE {6, 1};
	static constexpr BitField<u32, u8> DP_LINK_TRAINING_ENABLE {8, 3};
	static constexpr BitField<u32, bool> ENHANCED_FRAMING_ENABLE {18, 1};
	static constexpr BitField<u32, u8> TRAINING_PATTERN4_SELECT {19, 2};
	static constexpr BitField<u32, bool> FORCE_ACT {25, 1};
	static constexpr BitField<u32, u8> TRANSPORT_MODE_SELECT {27, 1};
	static constexpr BitField<u32, bool> FEC_ENABLE {30, 1};
	static constexpr BitField<u32, bool> TRANSPORT_ENABLE {31, 1};

	static constexpr u8 MODE_SST = 0;
	static constexpr u8 MODE_MST = 1;

	static constexpr u8 PATTERN4_4A = 0;
	static constexpr u8 PATTERN4_4B = 1;
	static constexpr u8 PATTERN4_4C = 0b10;

	static constexpr u8 PATTERN_1 = 0;
	static constexpr u8 PATTERN_2 = 1;
	static constexpr u8 PATTERN_IDLE = 0b10;
	static constexpr u8 PATTERN_NORMAL = 0b11;
	static constexpr u8 PATTERN_3 = 0b100;
	static constexpr u8 PATTERN_4 = 0b101;
}

namespace dp_tp_status {
	static constexpr BitField<u32, u8> PAYLOAD_MAPPING_VC0 {0, 2};
	static constexpr BitField<u32, u8> PAYLOAD_MAPPING_VC1 {4, 2};
	static constexpr BitField<u32, u8> PAYLOAD_MAPPING_VC2 {8, 2};
	static constexpr BitField<u32, u8> PAYLOAD_MAPPING_VC3 {12, 2};
	static constexpr BitField<u32, u8> STREAMS_ENABLED {16, 3};
	static constexpr BitField<u32, u8> DP_INIT_STATUS {19, 3};
	static constexpr BitField<u32, u8> DP_STREAM_STATUS {22, 1};
	static constexpr BitField<u32, u8> MODE_STATUS {23, 1};
	static constexpr BitField<u32, bool> ACT_SENT_STATUS {24, 1};
	static constexpr BitField<u32, bool> MIN_IDLES_SENT {25, 1};
	static constexpr BitField<u32, bool> ACTIVE_LINK_FRAME_STATUS {26, 1};
	static constexpr BitField<u32, bool> IDLE_LINK_FRAME_STATUS {27, 1};
	static constexpr BitField<u32, bool> FEC_ENABLE_LIVE_STATUS {28, 1};

	static constexpr u8 MODE_SST = 0;
	static constexpr u8 MODE_MST = 1;

	static constexpr u8 STREAM_INIT = 0;
	static constexpr u8 STREAM_ACTIVE = 1;

	static constexpr u8 INIT_PATTERN1 = 0;
	static constexpr u8 INIT_PATTERN2 = 1;
	static constexpr u8 INIT_PATTERN3 = 0b10;
	static constexpr u8 INIT_IDLE_SST = 0b100;
	static constexpr u8 INIT_IDLE_MST = 0b101;
	static constexpr u8 INIT_ACTIVE_SST = 0b110;
	static constexpr u8 INIT_ACTIVE_MST = 0b111;

	static constexpr u8 VC_TRANS_A = 0;
	static constexpr u8 VC_TRANS_B = 1;
	static constexpr u8 VC_TRANS_C = 0b10;
	static constexpr u8 VC_TRANS_D = 0b11;
}

namespace port_tx_dw4 {
	static constexpr BitField<u32, u8> CURSOR_COEFF {0, 6};
	static constexpr BitField<u32, u8> POST_CURSOR2 {6, 6};
	static constexpr BitField<u32, u8> POST_CURSOR1 {12, 6};
	static constexpr BitField<u32, bool> LOADGEN_SELECT {31, 1};
}

namespace port_cl_dw10 {
	static constexpr BitField<u32, u8> STATIC_POWER_DOWN {4, 4};

	static constexpr u8 NONE = 0;
	static constexpr u8 LANES_3_2 = 0b1100;
	static constexpr u8 LANES_3_2_1 = 0b1110;
	static constexpr u8 LANES_1_0 = 0b11;
	static constexpr u8 LANES_2_1_0 = 0b111;
	static constexpr u8 LANES_3_1_0 = 0b1011;
	static constexpr u8 LANES_3_1 = 0b1010;
	static constexpr u8 LANES_3 = 0b1000;
}

namespace ddi_buf_ctl {
	static constexpr BitField<u32, bool> INIT_DISPLAY_DETECTED {0, 1};
	static constexpr BitField<u32, u8> DP_PORT_WIDTH_SELECT {1, 3};
	static constexpr BitField<u32, bool> DDI_IDLE_STATUS {7, 1};
	static constexpr BitField<u32, u8> USBC_DP_LANE_STAGGERING_DELAY {8, 8};
	static constexpr BitField<u32, bool> PORT_REVERSAL {16, 1};
	static constexpr BitField<u32, bool> PHY_PARAM_ADJUST {28, 1};
	static constexpr BitField<u32, bool> OVERRIDE_TRAINING_ENABLE {29, 1};
	static constexpr BitField<u32, bool> DDI_BUFFER_ENABLE {31, 1};

	static constexpr u8 WIDTH_1 = 0;
	static constexpr u8 WIDTH_2 = 1;
	static constexpr u8 WIDTH_4 = 0b11;
}

namespace trans_conf {
	static constexpr BitField<u32, u8> DP_AUDIO_SYMBOL_WATERMARK {0, 7};
	static constexpr BitField<u32, u8> INTERLACED_MODE {21, 2};
	static constexpr BitField<u32, bool> TRANSCODER_STATE {30, 1};
	static constexpr BitField<u32, bool> TRANSCODER_ENABLE {31, 1};

	static constexpr u8 INTERLACED_PF_PD = 0;
	static constexpr u8 INTERLACED_PF_ID = 1;
	static constexpr u8 INTERLACED_IF_ID = 0b11;
}

namespace gmbus0 {
	static constexpr BitField<u32, u8> PIN_PAIR_SELECT {0, 5};
	static constexpr BitField<u32, bool> BYTE_COUNT_OVERRIDE {6, 1};
	static constexpr BitField<u32, u8> GMBUS_RATE_SELECT {8, 2};
	static constexpr BitField<u32, bool> CLOCK_STOP_ENABLE {14, 1};

	static constexpr u8 RATE_100KHZ = 0;
	static constexpr u8 RATE_50KHZ = 1;
}

namespace gmbus1 {
	static constexpr BitField<u32, u8> SECONDARY_ADDR {0, 8};
	static constexpr BitField<u32, u8> SECONDARY_REG_INDEX {8, 8};
	static constexpr BitField<u32, u16> TOTAL_BYTE_CNT {16, 9};
	static constexpr BitField<u32, u8> BUS_CYCLE_SELECT {25, 3};
	static constexpr BitField<u32, bool> ENABLE_TIMEOUT {29, 1};
	static constexpr BitField<u32, bool> SW_READY {30, 1};
	static constexpr BitField<u32, bool> SW_CLEAR_INT {31, 1};

	static constexpr u8 CYCLE_WAIT = 1;
	static constexpr u8 CYCLE_INDEX = 0b10;
	static constexpr u8 CYCLE_STOP = 0b100;

	static constexpr u8 ADDR_READ = 1;
	static constexpr u8 ADDR_WRITE = 0;
}

namespace gmbus2 {
	static constexpr BitField<u32, u16> CURRENT_BYTE_CNT {0, 9};
	static constexpr BitField<u32, bool> GMBUS_ACTIVE {9, 1};
	static constexpr BitField<u32, bool> NAK_INDICATOR {10, 1};
	static constexpr BitField<u32, bool> HW_READY {11, 1};
	static constexpr BitField<u32, bool> SECONDARY_STALL_TIMEOUT {13, 1};
	static constexpr BitField<u32, bool> HW_WAIT_PHASE {14, 1};
	static constexpr BitField<u32, bool> IN_USE {15, 1};
}

namespace trans_hblank {
	static constexpr BitField<u32, u16> HBLANK_START {0, 14};
	static constexpr BitField<u32, u16> HBLANK_END {16, 14};
}

namespace trans_hsync {
	static constexpr BitField<u32, u16> HSYNC_START {0, 14};
	static constexpr BitField<u32, u16> HSYNC_END {16, 14};
}

namespace trans_htotal {
	static constexpr BitField<u32, u16> HACTIVE {0, 14};
	static constexpr BitField<u32, u16> HTOTAL {16, 14};
}

namespace trans_msa {
	static constexpr BitField<u32, u8> MSA_MISC0 {0, 8};
	static constexpr BitField<u32, u8> MSA_MISC1 {8, 8};
}

namespace trans_mult {
	static constexpr BitField<u32, u8> MULTIPLIER {0, 3};

	static constexpr u8 X1 = 0;
	static constexpr u8 X2 = 1;
	static constexpr u8 X4 = 0b11;
}

namespace trans_push {
	static constexpr BitField<u32, bool> SEND_PUSH {30, 1};
	static constexpr BitField<u32, bool> PUSH_ENABLE {31, 1};
}

namespace trans_space {
	static constexpr BitField<u32, u16> VACTIVE_SPACE {0, 12};
}

namespace trans_vblank {
	static constexpr BitField<u32, u16> VBLANK_START {0, 13};
	static constexpr BitField<u32, u16> VBLANK_END {16, 13};
}

namespace trans_vsync {
	static constexpr BitField<u32, u16> VSYNC_START {0, 13};
	static constexpr BitField<u32, u16> VSYNC_END {16, 13};
}

namespace trans_vsyncshift {
	static constexpr BitField<u32, u16> SECOND_FIELD_VSYNC_SHIFT {0, 13};
}

namespace trans_vtotal {
	static constexpr BitField<u32, u16> VACTIVE {0, 13};
	static constexpr BitField<u32, u16> VTOTAL {16, 13};
}

namespace port_tx_dw5 {
	static constexpr BitField<u32, u8> RTERM_SELECT {3, 3};
	static constexpr BitField<u32, u8> CR_SCALING_COEF {11, 5};
	static constexpr BitField<u32, u8> DECODE_TIMER_SEL {16, 2};
	static constexpr BitField<u32, u8> SCALING_MODE_SEL {18, 3};
	static constexpr BitField<u32, bool> COEFF_POLARITY {25, 1};
	static constexpr BitField<u32, bool> CURSOR_PROGRAM {26, 1};
	static constexpr BitField<u32, bool> DISABLE_3TAP {29, 1};
	static constexpr BitField<u32, bool> DISABLE_2TAP {30, 1};
	static constexpr BitField<u32, bool> TX_TRAINING_ENABLE {31, 1};
}

namespace port_tx_dw7 {
	static constexpr BitField<u32, u8> N_SCALAR {24, 7};
}

namespace port_tx_dw2 {
	static constexpr BitField<u32, u8> RCOMP_SCALAR {0, 8};
	static constexpr BitField<u32, u8> SWING_SEL_LOWER {11, 3};
	static constexpr BitField<u32, u8> SWING_SEL_UPPER {15, 1};
}

namespace pipe_bottom_color {
	static constexpr BitField<u32, u16> B_BOTTOM_COLOR {0, 10};
	static constexpr BitField<u32, u16> G_BOTTOM_COLOR {10, 10};
	static constexpr BitField<u32, u16> R_BOTTOM_COLOR {20, 10};
}

namespace pipe_srcsz {
	static constexpr BitField<u32, u16> HEIGHT {0, 13};
	static constexpr BitField<u32, u16> WIDTH {16, 13};
}

namespace plane_ctl {
	static constexpr BitField<u32, u8> PLANE_ROTATION {0, 2};
	static constexpr BitField<u32, bool> ALLOW_DOUBLE_BUFFER_UPDATE_DISABLE {3, 1};
	static constexpr BitField<u32, bool> HORIZONTAL_FLIP {8, 1};
	static constexpr BitField<u32, bool> ASYNC_ADDRESS_UPDATE_ENABLE {9, 1};
	static constexpr BitField<u32, u8> TILED_SURFACE {10, 3};
	static constexpr BitField<u32, u8> RGB_COLOR_ORDER {20, 1};
	static constexpr BitField<u32, u8> SRC_PIXEL_FMT {23, 5};
	static constexpr BitField<u32, bool> PLANE_ENABLE {31, 1};

	static constexpr u8 FMT_RGB8888 = 0b1000;
	static constexpr u8 FMT_YUV444_PACKED_8BPC = 0b10000;

	static constexpr u8 ORDER_BGRX = 0;
	static constexpr u8 ORDER_RGBX = 1;

	static constexpr u8 TILE_LINEAR = 0;
	static constexpr u8 TILE_X = 1;
	static constexpr u8 TILE_Y = 0b100;

	static constexpr u8 ROTATE_NONE = 0;
	static constexpr u8 ROTATE_90 = 1;
	static constexpr u8 ROTATE_180 = 0b10;
	static constexpr u8 ROTATE_270 = 0b11;
}

namespace plane_size {
	static constexpr BitField<u32, u16> WIDTH {0, 13};
	static constexpr BitField<u32, u16> HEIGHT {16, 13};
}

namespace plane_stride {
	static constexpr BitField<u32, u16> STRIDE {0, 11};
}

namespace plane_surf {
	static constexpr BitField<u32, u32> SURFACE_BASE_ADDR {12, 20};
}

namespace plane_buf_cfg {
	static constexpr BitField<u32, u16> BUFFER_START {0, 11};
	static constexpr BitField<u32, u16> BUFFER_END {16, 11};
}

static bool irq_handler(IrqFrame*) {
	println("[kernel][intel]: irq");
	return true;
}

namespace {
	constexpr u64 GTT_PRESENT = 1;
}

#include "dev/gpu/gpu.hpp"
#include "dev/gpu/gpu_dev.hpp"
#include "op_region.hpp"
#include "vbt.hpp"

namespace {
	constexpr u8 DDI_PORT_A = 0;
	constexpr u8 DDI_PORT_B = 1;
	constexpr u8 DDI_PORT_C = 2;
	constexpr u8 DDI_PORT_D = 3;
	constexpr u8 DDI_PORT_E = 4;
	constexpr u8 DDI_PORT_F = 5;
	constexpr u8 DDI_PORT_G = 6;
	constexpr u8 DDI_PORT_H = 7;
	constexpr u8 DDI_PORT_I = 8;
}

static u8 dvo_port_to_ddi(u8 dvo_port) {
	switch (dvo_port) {
		case vbt::DVO_PORT_HDMIA:
		case vbt::DVO_PORT_DPA:
			return DDI_PORT_A;
		case vbt::DVO_PORT_HDMIB:
		case vbt::DVO_PORT_DPB:
			return DDI_PORT_B;
		case vbt::DVO_PORT_HDMIC:
		case vbt::DVO_PORT_DPC:
			return DDI_PORT_C;
		case vbt::DVO_PORT_HDMID:
		case vbt::DVO_PORT_DPD:
			return DDI_PORT_D;
		case vbt::DVO_PORT_HDMIE:
		case vbt::DVO_PORT_DPE:
		case vbt::DVO_PORT_CRT:
			return DDI_PORT_E;
		case vbt::DVO_PORT_HDMIF:
		case vbt::DVO_PORT_DPF:
			return DDI_PORT_F;
		case vbt::DVO_PORT_HDMIG:
		case vbt::DVO_PORT_DPG:
			return DDI_PORT_G;
		case vbt::DVO_PORT_HDMIH:
		case vbt::DVO_PORT_DPH:
			return DDI_PORT_H;
		case vbt::DVO_PORT_HDMII:
		case vbt::DVO_PORT_DPI:
			return DDI_PORT_I;
		default:
			assert(false);
	}
}

enum class PortType {
	Hdmi
};

struct Port {
	PortType type;
};

struct [[gnu::packed]] StandardTimingDescriptor {
	u8 first;
	u8 flags;
};

struct [[gnu::packed]] DetailedTimingDescriptor {
	u16 pixel_clock;
	u8 h_active_pixels_lsb;
	u8 h_blank_pixels_lsb;
	u8 h_flags;
	u8 v_active_lines_lsb;
	u8 v_blank_lines_lsb;
	u8 v_flags;
	u8 h_sync_offset_pixels_lsb;
	u8 h_sync_pulse_width_pixels_lsb;
	u8 v_sync;
	u8 sync_flags;
	u8 h_image_size_mm_lsb;
	u8 v_image_size_mm_lsb;
	u8 image_size_flags;
	u8 h_border_pixels;
	u8 v_border_lines;
	u8 features;

	[[nodiscard]] constexpr u16 h_active_pixels() const {
		return h_active_pixels_lsb | (h_flags >> 4 << 8);
	}

	[[nodiscard]] constexpr u16 h_blank_pixels() const {
		return h_blank_pixels_lsb | (h_flags & 0xF) << 8;
	}

	[[nodiscard]] constexpr u16 v_active_lines() const {
		return v_active_lines_lsb | (v_flags >> 4 << 8);
	}

	[[nodiscard]] constexpr u16 v_blank_lines() const {
		return v_blank_lines_lsb | (v_flags & 0xF) << 8;
	}

	[[nodiscard]] constexpr u8 v_sync_offset_lines() const {
		return v_sync >> 4 | (sync_flags >> 2 & 0b11) << 4;
	}

	[[nodiscard]] constexpr u8 v_sync_pulse_width_lines() const {
		return (v_sync & 0xF) | (sync_flags & 0b11) << 4;
	}

	[[nodiscard]] constexpr u16 h_sync_offset_pixels() const {
		return h_sync_offset_pixels_lsb >> 4 | (sync_flags >> 6) << 8;
	}

	[[nodiscard]] constexpr u16 h_sync_pulse_width_pixels() const {
		return h_sync_pulse_width_pixels_lsb | (sync_flags >> 4 & 0b11) << 8;
	}

	[[nodiscard]] constexpr u16 h_image_size_mm() const {
		return h_image_size_mm_lsb >> 4 | (image_size_flags >> 4) << 8;
	}

	[[nodiscard]] constexpr u16 v_image_size_mm() const {
		return v_image_size_mm_lsb | (image_size_flags & 0b1111) << 8;
	}
};

struct [[gnu::packed]] Edid {
	u8 signature[8];
	u16 manufacturer_id;
	u16 product_code;
	u32 serial_number;
	u8 manufacture_week;
	u8 manufacture_year;
	u8 edid_version;
	u8 edid_revision;
	u8 video_input_params;
	u8 horizontal_screen_size;
	u8 vertical_screen_size;
	u8 display_gamma;
	u8 supported_features;
	u8 chroma_red_green_lsb;
	u8 chroma_blue_white_lsb;
	u8 chroma_red_x_msb;
	u8 chroma_red_y_msb;
	u8 chroma_green_x_msb;
	u8 chroma_green_y_msb;
	u8 chroma_blue_x_msb;
	u8 chroma_blue_y_msb;
	u8 chroma_default_white_point_x_msb;
	u8 chroma_default_white_point_y_msb;
	u8 timing_bitmap0;
	u8 timing_bitmap1;
	u8 timing_bitmap2;
	StandardTimingDescriptor std_timing[8];
	DetailedTimingDescriptor detailed_timing[4];
	u8 num_of_extensions;
	u8 checksum;
};

static constexpr struct {
	u8 swing_sel_dw2;
	u8 n_scalar_dw7;
	u8 cursor_coeff_dw4;
	u8 post_cursor1_dw4;
} tgl_trans_hdmi[] {
	{.swing_sel_dw2 = 0b1010, .n_scalar_dw7 = 0x60, .cursor_coeff_dw4 = 0x3F, .post_cursor1_dw4 = 0},
	{.swing_sel_dw2 = 0b1011, .n_scalar_dw7 = 0x73, .cursor_coeff_dw4 = 0x36, .post_cursor1_dw4 = 0x9},
	{.swing_sel_dw2 = 0b0110, .n_scalar_dw7 = 0x7F, .cursor_coeff_dw4 = 0x31, .post_cursor1_dw4 = 0xE},
	{.swing_sel_dw2 = 0b1011, .n_scalar_dw7 = 0x73, .cursor_coeff_dw4 = 0x3F, .post_cursor1_dw4 = 0},
	{.swing_sel_dw2 = 0b0110, .n_scalar_dw7 = 0x7F, .cursor_coeff_dw4 = 0x37, .post_cursor1_dw4 = 0x8},
	{.swing_sel_dw2 = 0b0110, .n_scalar_dw7 = 0x7F, .cursor_coeff_dw4 = 0x3F, .post_cursor1_dw4 = 0},
	{.swing_sel_dw2 = 0b0110, .n_scalar_dw7 = 0x7F, .cursor_coeff_dw4 = 0x35, .post_cursor1_dw4 = 0xA},
};

struct IntelSurface : public GpuSurface {
	IntelSurface(usize gtt_phys, usize phys) : gtt_phys {gtt_phys}, phys {phys} {}

	usize get_phys() override {
		return phys;
	}

	usize gtt_phys;
	usize phys;
};

struct IntelGpu : public Gpu {
	IntelGpu(IoSpace space, IoSpace gtt_space, usize stolen_mem_start)
			: space {space}, gtt_space {gtt_space}, stolen_mem_start {stolen_mem_start} {
		width = (space.load(regs::PIPE_SRCSZ_A) & pipe_srcsz::WIDTH) + 1;
		height = (space.load(regs::PIPE_SRCSZ_A) & pipe_srcsz::HEIGHT) + 1;
		stride = (space.load(regs::PLANE_STRIDE_1_A) & plane_stride::STRIDE) * 64;
		owns_boot_fb = true;
		supports_page_flipping = true;
	}

	GpuSurface* create_surface() override {
		usize gtt_phys = 0;
		usize phys;
		if (gtt_surf_addr_offset == 0) {
			phys = stolen_mem_start;
		}
		else {
			usize pages = ALIGNUP(stride * height, PAGE_SIZE) / PAGE_SIZE;
			phys = pmalloc(pages);
			assert(phys);

			usize gtt_entry_start = gtt_surf_addr_offset / PAGE_SIZE;
			for (usize i = 0; i < pages; ++i) {
				u64 entry = (pages + i * PAGE_SIZE) | GTT_PRESENT;
				gtt_space.store((gtt_entry_start + i) * 8, entry);
			}

			gtt_phys = gtt_surf_addr_offset;
		}
		gtt_surf_addr_offset += ALIGNUP(stride * height, PAGE_SIZE);

		return new IntelSurface {gtt_phys, phys};
	}

	void destroy_surface(GpuSurface* surface) override {
		delete static_cast<IntelSurface*>(surface);
	}

	void flip(GpuSurface* surface) override {
		auto* intel = static_cast<IntelSurface*>(surface);
		auto surf = space.load(regs::PLANE_SURF_1_A);
		surf &= ~plane_surf::SURFACE_BASE_ADDR;
		surf |= plane_surf::SURFACE_BASE_ADDR(intel->phys >> 12);
		space.store(regs::PLANE_SURF_1_A, surf);
	}

	IoSpace space;
	IoSpace gtt_space;
	u32 gtt_surf_addr_offset {};
	u32 width;
	u32 height;
	u32 stride;
	usize stolen_mem_start;
};

static InitStatus intel_init(pci::Device& device) {
	println("[kernel]: intel gpu init");

	/*auto _bar_size = device.get_bar_size(0);

	IoSpace _space {device.map_bar(0)};
	IoSpace _gtt_space = _space.subspace(_bar_size / 2);

	usize stolen_mem_start = device.get_bar(2);

	auto* gpu = new IntelGpu {_space, _gtt_space, stolen_mem_start};

	auto gpu_device = kstd::make_shared<GpuDevice>(gpu, "intel gpu");
	dev_add(std::move(gpu_device), CrescentDeviceType::Gpu);

	return InitStatus::Success;*/

	kstd::vector<u8> vbt_vec;
	if (auto vbt_file = qemu_fw_cfg_get_file("opt/intel-vbt")) {
		vbt_vec = std::move(vbt_file).value();
	}
	else {
		u32 asls_phys = device.read32(0xFC);
		assert(asls_phys);
		auto* op_region_hdr = to_virt<OpRegionHeader>(asls_phys);

		if (kstd::string_view {op_region_hdr->signature, 16} != "IntelGraphicsMem") {
			println("[kernel][intel]: invalid OpRegion signature");
			return InitStatus::Success;
		}

		const OpRegionAsle* asle = nullptr;
		if (op_region_hdr->mbox & 1 << 2) {
			asle = offset(op_region_hdr, const OpRegionAsle*, 0x300);
		}

		if (op_region_hdr->major_version() >= 2 && asle && asle->rvda && asle->rvds) {
			u64 rvda = asle->rvda;

			if (op_region_hdr->major_version() > 2 || (op_region_hdr->major_version() == 2 && op_region_hdr->minor_version() >= 1)) {
				rvda += asls_phys;
			}

			vbt_vec.resize(asle->rvds);
			memcpy(vbt_vec.data(), to_virt<void>(rvda), asle->rvds);
		}
		else {
			assert((op_region_hdr->mbox & 1 << 3) && "VBT must be supported");

			u32 vbt_size = ((op_region_hdr->mbox & 1 << 4) ? 0x1C00 : 0x2000) - 0x400;

			vbt_vec.resize(vbt_size);
			memcpy(vbt_vec.data(), offset(op_region_hdr, void*, 0x400), vbt_size);
		}
	}

	auto* vbt_hdr = reinterpret_cast<const vbt::Header*>(vbt_vec.data());
	if (kstd::string_view {vbt_hdr->signature, 4} != vbt::VBT_SIGNATURE) {
		println("[kernel][intel]: invalid VBT signature");
		return InitStatus::Success;
	}
	auto* bdb = offset(vbt_hdr, const vbt::BdbHeader*, vbt_hdr->bdb_offset);
	if (kstd::string_view {bdb->signature, 16} != vbt::BDB_SIGNATURE) {
		println("[kernel][intel]: invalid BDB signature");
		return InitStatus::Success;
	}

	auto* general_defs = static_cast<const vbt::BdbGeneralDefinitions*>(vbt::get_block(bdb, vbt::BdbBlockId::GeneralDefinitions));
	auto* driver_features = static_cast<const vbt::BdbDriverFeatures*>(vbt::get_block(bdb, vbt::BdbBlockId::DriverFeatures));

	assert(general_defs->child_dev_size);
	u32 child_count =
		(vbt::get_block_size(general_defs) - (sizeof(*general_defs) - sizeof(general_defs->common)))
		/ general_defs->child_dev_size;

	println("[kernel][intel]: found ", child_count, " vbt children");

	u8 hdmi_ddi_port = 0;
	u8 hdmi_ddc_pin = 0;
	u8 hdmi_level_shifter_value = 0;

	for (u32 i = 0; i < child_count; ++i) {
		vbt::ChildDevice child {};
		memcpy(
			&child,
			general_defs->devices + i * general_defs->child_dev_size,
			kstd::min(general_defs->child_dev_size, static_cast<u8>(sizeof(vbt::ChildDevice))));

		if (!child.device_type) {
			continue;
		}

		u8 ddi_port = dvo_port_to_ddi(child.dvo_port);
		bool is_dp = child.device_type & vbt::DEVICE_TYPE_DISPLAYPORT_OUTPUT;
		bool is_hdmi = child.device_type == vbt::DEVICE_TYPE_HDMI;

		if (is_hdmi) {
			println("hdmi found on ddi port ", ddi_port);
			hdmi_ddi_port = ddi_port;
			hdmi_ddc_pin = child.ddc_pin;
			if (vbt_hdr->version < 158) {
				hdmi_level_shifter_value = sizeof(tgl_trans_hdmi) / sizeof(*tgl_trans_hdmi) - 1;
			}
			else {
				hdmi_level_shifter_value = child.hdmi_level_shifter_value;
			}
			assert(!child.iboost);
			break;
		}
	}

	/*auto pcie = static_cast<pci::caps::Pcie*>(device.hdr0->get_cap(pci::Cap::PciExpress, 0));
	assert(pcie);
	assert(pcie->supports_function_reset());

	pcie->init_function_reset();
	mdelay(100);*/

	auto bar_size = device.get_bar_size(0);

	IoSpace space {device.map_bar(0)};
	IoSpace gtt_space = space.subspace(bar_size / 2);

	print(Fmt::Hex);

	device.enable_io_space(true);
	device.enable_mem_space(true);
	device.enable_bus_master(true);

	auto nde_rstwrn_opt = space.load(regs::NDE_RSTWRN_OPT);
	nde_rstwrn_opt |= nde_rstwrn_opt::RST_PCH_HANDSHAKE_EN(true);
	space.store(regs::NDE_RSTWRN_OPT, nde_rstwrn_opt);

	auto status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG0_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG0 dist status");
		return InitStatus::Success;
	}

	auto pwr_well_ctl = space.load(regs::PWR_WELL_CTL2);
	pwr_well_ctl |= pwr_well_ctl::POWER_WELL_1_REQ(true);
	space.store(regs::PWR_WELL_CTL2, pwr_well_ctl);

	status = with_timeout([&]() {
		return space.load(regs::PWR_WELL_CTL2) & pwr_well_ctl::POWER_WELL_1_STATE;
	}, 45);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for power well 1 state");
		return InitStatus::Success;
	}

	status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG1_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG1 dist status");
		return InitStatus::Success;
	}

	while (true) {
		space.store(regs::GTDRIVER_MAILBOX_DATA0, 0x3);
		space.store(regs::GTDRIVER_MAILBOX_DATA1, 0);
		space.store(regs::GTDRIVER_MAILBOX_INTERFACE, 0x80000007);
		status = with_timeout([&]() {
			return !(space.load(regs::GTDRIVER_MAILBOX_INTERFACE) & gt_driver_mailbox::BUSY);
		}, 150);
		if (!status) {
			println("[kernel][intel]: timeout while setting voltage to max");
			return InitStatus::Success;
		}

		if (space.load(regs::GTDRIVER_MAILBOX_DATA0) & 1) {
			break;
		}
	}

	auto ref_freq = space.load(regs::DSSM) & dssm::REF_FREQ;
	assert(ref_freq == dssm::FREQ_38_4);
	// 192mhz
	u8 pll_ratio = 10;

	auto cdclk_pll_enable = space.load(regs::CDCLK_PLL_ENABLE);
	cdclk_pll_enable |= cdclk_pll_enable::PLL_RATIO(pll_ratio);
	space.store(regs::CDCLK_PLL_ENABLE, cdclk_pll_enable);

	cdclk_pll_enable = space.load(regs::CDCLK_PLL_ENABLE);
	cdclk_pll_enable |= cdclk_pll_enable::PLL_ENABLE(true);
	space.store(regs::CDCLK_PLL_ENABLE, cdclk_pll_enable);

	status = with_timeout([&]() {
		return space.load(regs::CDCLK_PLL_ENABLE) & cdclk_pll_enable::PLL_LOCK;
	}, 200);
	if (!status) {
		println("[kernel][intel]: timeout while locking pll");
		return InitStatus::Success;
	}

	auto cdclk_ctl = space.load(regs::CDCLK_CTL);
	cdclk_ctl &= ~cdclk_ctl::CD_FREQ_DECIMAL;
	cdclk_ctl &= ~cdclk_ctl::CD2X_DIVIDER;
	cdclk_ctl |= cdclk_ctl::CD2X_DIVIDER(cdclk_ctl::DIV_BY_1);
	cdclk_ctl |= cdclk_ctl::CD_FREQ_DECIMAL(cdclk_ctl::FREQ_192);
	space.store(regs::CDCLK_CTL, cdclk_ctl);

	// level 0
	space.store(regs::GTDRIVER_MAILBOX_DATA0, 0x0);
	space.store(regs::GTDRIVER_MAILBOX_DATA1, 0x0);
	space.store(regs::GTDRIVER_MAILBOX_INTERFACE, 0x80000007);

	auto dbuf_ctl_s1 = space.load(regs::DBUF_CTL_S1);
	dbuf_ctl_s1 |= dbuf_ctl::POWER_REQ(true);
	space.store(regs::DBUF_CTL_S1, dbuf_ctl_s1);
	status = with_timeout([&]() {
		return space.load(regs::DBUF_CTL_S1) & dbuf_ctl::POWER_STATE;
	}, 10);
	if (!status) {
		println("[kernel][intel]: timeout while enabling dbuf s1 power");
		return InitStatus::Success;
	}

	auto mbus_abox_ctl = space.load(regs::MBUS_ABOX_CTL);
	mbus_abox_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL1;
	mbus_abox_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL2;
	mbus_abox_ctl &= ~mbus_abox_ctl::B_CREDITS;
	mbus_abox_ctl &= ~mbus_abox_ctl::BW_CREDITS;
	mbus_abox_ctl |= mbus_abox_ctl::BT_CREDITS_POOL1(16);
	mbus_abox_ctl |= mbus_abox_ctl::BT_CREDITS_POOL2(16);
	mbus_abox_ctl |= mbus_abox_ctl::B_CREDITS(1);
	mbus_abox_ctl |= mbus_abox_ctl::BW_CREDITS(1);
	space.store(regs::MBUS_ABOX_CTL, mbus_abox_ctl);

	auto mbus_abox1_ctl = space.load(regs::MBUS_ABOX1_CTL);
	mbus_abox1_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL1;
	mbus_abox1_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL2;
	mbus_abox1_ctl &= ~mbus_abox_ctl::B_CREDITS;
	mbus_abox1_ctl &= ~mbus_abox_ctl::BW_CREDITS;
	mbus_abox1_ctl |= mbus_abox_ctl::BT_CREDITS_POOL1(16);
	mbus_abox1_ctl |= mbus_abox_ctl::BT_CREDITS_POOL2(16);
	mbus_abox1_ctl |= mbus_abox_ctl::B_CREDITS(1);
	mbus_abox1_ctl |= mbus_abox_ctl::BW_CREDITS(1);
	space.store(regs::MBUS_ABOX1_CTL, mbus_abox1_ctl);

	auto mbus_abox2_ctl = space.load(regs::MBUS_ABOX2_CTL);
	mbus_abox2_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL1;
	mbus_abox2_ctl &= ~mbus_abox_ctl::BT_CREDITS_POOL2;
	mbus_abox2_ctl &= ~mbus_abox_ctl::B_CREDITS;
	mbus_abox2_ctl &= ~mbus_abox_ctl::BW_CREDITS;
	mbus_abox2_ctl |= mbus_abox_ctl::BT_CREDITS_POOL1(16);
	mbus_abox2_ctl |= mbus_abox_ctl::BT_CREDITS_POOL2(16);
	mbus_abox2_ctl |= mbus_abox_ctl::B_CREDITS(1);
	mbus_abox2_ctl |= mbus_abox_ctl::BW_CREDITS(1);
	space.store(regs::MBUS_ABOX2_CTL, mbus_abox2_ctl);

	dbuf_ctl_s1 = space.load(regs::DBUF_CTL_S1);
	dbuf_ctl_s1 &= ~dbuf_ctl::TRACKER_STATE_SERVICE;
	dbuf_ctl_s1 |= dbuf_ctl::TRACKER_STATE_SERVICE(8);
	space.store(regs::DBUF_CTL_S1, dbuf_ctl_s1);

	auto buddy1_ctl = space.load(regs::BW_BUDDY1_CTL);
	buddy1_ctl |= bw_buddy_ctl::BUDDY_DISABLE(true);
	space.store(regs::BW_BUDDY1_CTL, buddy1_ctl);
	auto buddy2_ctl = space.load(regs::BW_BUDDY2_CTL);
	buddy2_ctl |= bw_buddy_ctl::BUDDY_DISABLE(true);
	space.store(regs::BW_BUDDY2_CTL, buddy2_ctl);

	pwr_well_ctl = space.load(regs::PWR_WELL_CTL2);
	pwr_well_ctl |= pwr_well_ctl::POWER_WELL_2_REQ(true);
	space.store(regs::PWR_WELL_CTL2, pwr_well_ctl);

	status = with_timeout([&]() {
		return space.load(regs::PWR_WELL_CTL2) & pwr_well_ctl::POWER_WELL_2_STATE;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for power well 2 state");
		return InitStatus::Success;
	}

	status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG2_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG2 dist status");
		return InitStatus::Success;
	}

	pwr_well_ctl = space.load(regs::PWR_WELL_CTL2);
	pwr_well_ctl |= pwr_well_ctl::POWER_WELL_3_REQ(true);
	space.store(regs::PWR_WELL_CTL2, pwr_well_ctl);

	status = with_timeout([&]() {
		return space.load(regs::PWR_WELL_CTL2) & pwr_well_ctl::POWER_WELL_3_STATE;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for power well 3 state");
		return InitStatus::Success;
	}

	status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG3_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG3 dist status");
		return InitStatus::Success;
	}

	pwr_well_ctl = space.load(regs::PWR_WELL_CTL2);
	pwr_well_ctl |= pwr_well_ctl::POWER_WELL_4_REQ(true);
	space.store(regs::PWR_WELL_CTL2, pwr_well_ctl);

	status = with_timeout([&]() {
		return space.load(regs::PWR_WELL_CTL2) & pwr_well_ctl::POWER_WELL_4_STATE;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for power well 4 state");
		return InitStatus::Success;
	}

	status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG4_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG4 dist status");
		return InitStatus::Success;
	}

	pwr_well_ctl = space.load(regs::PWR_WELL_CTL2);
	pwr_well_ctl |= pwr_well_ctl::POWER_WELL_5_REQ(true);
	space.store(regs::PWR_WELL_CTL2, pwr_well_ctl);

	status = with_timeout([&]() {
		return space.load(regs::PWR_WELL_CTL2) & pwr_well_ctl::POWER_WELL_5_STATE;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for power well 5 state");
		return InitStatus::Success;
	}

	status = with_timeout([&]() {
		return space.load(regs::FUSE_STATUS) & fuse_status::FUSE_PG5_DIST_STATUS;
	}, 20);
	if (!status) {
		println("[kernel][intel]: timeout while waiting for PG5 dist status");
		return InitStatus::Success;
	}

	println("pipe sizes:");

	println((space.load(regs::PIPE_SRCSZ_A) >> 16 & 0b1111111111111) + 1, "x", (space.load(regs::PIPE_SRCSZ_A) & 0b1111111111111) + 1);
	println((space.load(regs::PIPE_SRCSZ_B) >> 16 & 0b1111111111111) + 1, "x", (space.load(regs::PIPE_SRCSZ_B) & 0b1111111111111) + 1);
	println((space.load(regs::PIPE_SRCSZ_C) >> 16 & 0b1111111111111) + 1, "x", (space.load(regs::PIPE_SRCSZ_C) & 0b1111111111111) + 1);
	println((space.load(regs::PIPE_SRCSZ_D) >> 16 & 0b1111111111111) + 1, "x", (space.load(regs::PIPE_SRCSZ_D) & 0b1111111111111) + 1);

	u32 width = (space.load(regs::PIPE_SRCSZ_A) >> 16 & 0b1111111111111) + 1;
	u32 height = (space.load(regs::PIPE_SRCSZ_A) & 0b1111111111111) + 1;
	u32 stride = (space.load(regs::PLANE_STRIDE_1_A) & 0b11111111111) * 64;
	u32 fb_size = height * stride;

	// 0x3FFFFFFFF

	//println("planes enabled:");
	//println(space.load(regs::PLANE_CTL_1_A) >> 31);
	//println(space.load(regs::PLANE_CTL_2_A) >> 31);
	//println(space.load(regs::PLANE_CTL_3_A) >> 31);
	//println(space.load(regs::PLANE_CTL_4_A) >> 31);
	//println(space.load(regs::PLANE_CTL_5_A) >> 31);
	//println(space.load(regs::PLANE_CTL_6_A) >> 31);
	//println(space.load(regs::PLANE_CTL_7_A) >> 31);

	//auto plane_surf = space.load(regs::PLANE_SURF_1_A);
	//plane_surf &= 0xFFF;
	//plane_surf |= ALIGNUP(stride * height, 0x1000);
	//space.store(regs::PLANE_SURF_1_A, plane_surf);

	auto display_int_ctl = space.load(regs::DISPLAY_INT_CTL);
	display_int_ctl |= display_int_ctl::ENABLE(true);
	space.store(regs::DISPLAY_INT_CTL, display_int_ctl);

	auto gfx_mstr_intr = space.load(regs::GFX_MSTR_INTR);
	gfx_mstr_intr |= gfx_mstr_intr::PRIMARY_INT(true);
	gfx_mstr_intr |= gfx_mstr_intr::DISPLAY(true);
	space.store(regs::GFX_MSTR_INTR, gfx_mstr_intr);

	auto status = device.alloc_irqs(1, 1, pci::IrqFlags::All);
	assert(status);
	auto irq = device.get_irq(0);
	register_irq_handler(
		irq,
		new IrqHandler {
			.fn {irq_handler},
			.can_be_shared = false
		});
	device.enable_irqs(true);
	device.enable_legacy_irq(true);

	auto sde_imr = space.load(regs::SDEIMR);
	sde_imr &= ~sinterrupt::DDI_A_HOTPLUG;
	sde_imr &= ~sinterrupt::DDI_B_HOTPLUG;
	sde_imr &= ~sinterrupt::DDI_C_HOTPLUG;
	sde_imr &= ~sinterrupt::DDI_D_HOTPLUG;
	space.store(regs::SDEIMR, sde_imr);

	auto sde_ier = space.load(regs::SDEIER);
	sde_ier |= sinterrupt::DDI_A_HOTPLUG(true);
	sde_ier |= sinterrupt::DDI_B_HOTPLUG(true);
	sde_ier |= sinterrupt::DDI_C_HOTPLUG(true);
	sde_ier |= sinterrupt::DDI_D_HOTPLUG(true);
	space.store(regs::SDEIER, sde_ier);

	auto shotplug = space.load(regs::SHOTPLUG_CTL);
	shotplug |= shotplug_ctl::DDI_A_HPD_ENABLE(true);
	shotplug |= shotplug_ctl::DDI_B_HPD_ENABLE(true);
	shotplug |= shotplug_ctl::DDI_C_HPD_ENABLE(true);
	shotplug |= shotplug_ctl::DDI_D_HPD_ENABLE(true);
	shotplug &= ~shotplug_ctl::DDI_A_HPD_STATUS;
	shotplug &= ~shotplug_ctl::DDI_B_HPD_STATUS;
	shotplug &= ~shotplug_ctl::DDI_C_HPD_STATUS;
	shotplug &= ~shotplug_ctl::DDI_D_HPD_STATUS;
	space.store(regs::SHOTPLUG_CTL, shotplug);
	
	println("shotplug: ", space.load(regs::SHOTPLUG_CTL));

	println("pending: ", space.load(regs::DISPLAY_INT_CTL) & display_int_ctl::DE_PCH_INTERRUPTS_PENDING);
	println("sdeisr: ", space.load(regs::SDEISR));
	println("sdeiir: ", space.load(regs::SDEIIR));

	auto gmbus_read = [&](u8* data, u8 pin, u8 addr, u16 count) {
		assert(count <= 511);

		auto gmbus1 = space.load(regs::GMBUS1);
		gmbus1 &= ~gmbus1::SW_READY;
		gmbus1 &= ~gmbus1::ENABLE_TIMEOUT;
		gmbus1 &= ~gmbus1::BUS_CYCLE_SELECT;
		gmbus1 &= ~gmbus1::TOTAL_BYTE_CNT;
		gmbus1 &= ~gmbus1::SECONDARY_REG_INDEX;
		gmbus1 &= ~gmbus1::SECONDARY_ADDR;
		space.store(regs::GMBUS1, gmbus1);

		gmbus1 = space.load(regs::GMBUS1);
		gmbus1 |= gmbus1::SW_CLEAR_INT(true);
		space.store(regs::GMBUS1, gmbus1);
		gmbus1 = space.load(regs::GMBUS1);
		gmbus1 &= ~gmbus1::SW_CLEAR_INT;
		space.store(regs::GMBUS1, gmbus1);

		auto gmbus0 = space.load(regs::GMBUS0);
		gmbus0 &= ~gmbus0::PIN_PAIR_SELECT;
		gmbus0 &= ~gmbus0::BYTE_COUNT_OVERRIDE;
		gmbus0 &= ~gmbus0::GMBUS_RATE_SELECT;
		gmbus0 &= ~gmbus0::CLOCK_STOP_ENABLE;
		gmbus0 |= gmbus0::PIN_PAIR_SELECT(pin);
		gmbus0 |= gmbus0::GMBUS_RATE_SELECT(gmbus0::RATE_100KHZ);
		space.store(regs::GMBUS0, gmbus0);

		gmbus1 = space.load(regs::GMBUS1);
		gmbus1 &= ~gmbus1::SECONDARY_ADDR;
		gmbus1 &= ~gmbus1::TOTAL_BYTE_CNT;
		gmbus1 &= ~gmbus1::BUS_CYCLE_SELECT;
		gmbus1 &= ~gmbus1::ENABLE_TIMEOUT;
		gmbus1 |= gmbus1::SECONDARY_ADDR(addr << 1 | gmbus1::ADDR_READ);
		gmbus1 |= gmbus1::TOTAL_BYTE_CNT(count);
		gmbus1 |= gmbus1::BUS_CYCLE_SELECT(gmbus1::CYCLE_INDEX | gmbus1::CYCLE_WAIT);
		gmbus1 |= gmbus1::SW_READY(true);
		space.store(regs::GMBUS1, gmbus1);

		for (int i = 0; i < count; i += 4) {
			while (!(space.load(regs::GMBUS2) & gmbus2::HW_READY));

			u32 value = space.load(regs::GMBUS3);
			memcpy(data + i, &value, kstd::min(count - i, 4));
		}

		gmbus1 = space.load(regs::GMBUS1);
		gmbus1 &= ~gmbus1::SECONDARY_ADDR;
		gmbus1 &= ~gmbus1::TOTAL_BYTE_CNT;
		gmbus1 &= ~gmbus1::BUS_CYCLE_SELECT;
		gmbus1 &= ~gmbus1::ENABLE_TIMEOUT;
		gmbus1 |= gmbus1::SW_READY(true);
		gmbus1 |= gmbus1::BUS_CYCLE_SELECT(gmbus1::CYCLE_STOP);
		space.store(regs::GMBUS1, gmbus1);
	};

	DetailedTimingDescriptor* timing_desc = nullptr;

	Edid edid {};
	gmbus_read(reinterpret_cast<u8*>(&edid), hdmi_ddc_pin, 0x50, 128);
	constexpr u8 EDID_SIGNATURE[] {0x0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0};
	assert(memcmp(edid.signature, EDID_SIGNATURE, 8) == 0);

	for (auto& desc : edid.detailed_timing) {
		if (desc.pixel_clock == 0) {
			continue;
		}
		timing_desc = &desc;
		break;
	}

	assert(timing_desc);

	// todo enable power wells

	auto dpll1_enable = space.load(regs::DPLL1_ENABLE);
	dpll1_enable |= dpll_enable::POWER_ENABLE(true);
	space.store(regs::DPLL1_ENABLE, dpll1_enable);
	while (!(space.load(regs::DPLL1_ENABLE) & dpll_enable::POWER_STATE));

	auto dpll1_ssc = space.load(regs::DPLL1_SSC);
	dpll1_ssc &= ~dpll_ssc::SSC_EN;
	space.store(regs::DPLL1_SSC, dpll1_ssc);

	u8 edid_bpc = 8;
	if (edid.edid_revision >= 4 &&
		(edid.video_input_params >> 4 & 0b111) != 0 &&
		(edid.video_input_params >> 4 & 0b111) != 0b111) {
		edid_bpc = 4 + (2 * (edid.video_input_params >> 4 & 0b111));
	}

	{
		u16 freq_mhz = timing_desc->pixel_clock / 100;
		if (edid_bpc == 10) {
			freq_mhz += freq_mhz / 4 * 5;
		}
		else if (edid_bpc == 12) {
			freq_mhz += freq_mhz / 2 * 3;
		}

		constexpr u8 DIVIDERS[] {
			2, 4, 6, 8, 10, 12, 14, 16, 18, 20,
			24, 28, 30, 32, 36, 40, 42, 44, 48,
			50, 52, 54, 56, 60, 64, 66, 68, 70,
			72, 76, 78, 80, 84, 88, 90, 92, 96,
			98, 100, 102, 3, 5, 7, 9, 15, 21
		};
		constexpr u16 DCO_MIN = 7998;
		constexpr u16 DCO_MAX = 10000;
		constexpr u16 DCO_MID = (DCO_MIN + DCO_MAX) / 2;
		u16 afe_clk = 5 * freq_mhz;
		int best_center = 0xFFFF;
		u8 best_div = 0;
		u16 best_dco = 0;

		for (auto div : DIVIDERS) {
			u16 dco = afe_clk * div;
			if (dco >= DCO_MIN && dco <= DCO_MAX) {
				int center = dco - DCO_MID;
				if (center < 0) {
					center *= -1;
				}
				if (center < best_center) {
					best_center = center;
					best_div = div;
					best_dco = dco;
				}
			}
		}

		u8 pdiv = 0;
		u8 qdiv = 0;
		u8 kdiv = 0;

		assert(best_div);
		if (best_div % 2 == 0) {
			if (best_div == 2) {
				pdiv = 2;
				qdiv = 1;
				kdiv = 1;
			}
			else if (best_div % 4 == 0) {
				pdiv = 2;
				qdiv = best_div / 4;
				kdiv = 2;
			}
			else if (best_div % 6 == 0) {
				pdiv = 3;
				qdiv = best_div / 6;
				kdiv = 2;
			}
			else if (best_div % 5 == 0) {
				pdiv = 5;
				qdiv = best_div / 10;
				kdiv = 2;
			}
			else if (best_div % 14 == 0) {
				pdiv = 7;
				qdiv = best_div / 14;
				kdiv = 2;
			}
		}
		else {
			if (best_div == 3 || best_div == 5 || best_div == 7) {
				pdiv = best_div;
				qdiv = 1;
				kdiv = 1;
			}
			else {
				pdiv = best_div / 3;
				qdiv = 1;
				kdiv = 3;
			}
		}

		u64 dco_integer = best_dco * 65536;
		// 19.2mhz ref freq (dpll divides 38.4mhz by 2 automatically)
		u64 dco_divider = 1258291;
		u64 dco_whole = dco_integer / dco_divider;
		u64 dco_fract = dco_integer % dco_divider / 65536;

		dco_whole &= 0x3FF;
		dco_fract &= 0x7FFF;

		auto dpll1_cfgcr0 = space.load(regs::DPLL1_CFGCR0);
		dpll1_cfgcr0 &= ~dpll_cfgcr0::DCO_INTEGER;
		dpll1_cfgcr0 &= ~dpll_cfgcr0::DCO_FRACTION;
		dpll1_cfgcr0 |= dpll_cfgcr0::DCO_INTEGER(dco_whole);
		dpll1_cfgcr0 |= dpll_cfgcr0::DCO_FRACTION(dco_fract);
		space.store(regs::DPLL1_CFGCR0, dpll1_cfgcr0);

		auto dpll1_cfgcr1 = space.load(regs::DPLL1_CFGCR1);
		dpll1_cfgcr1 &= ~dpll_cfgcr1::PDIV;
		dpll1_cfgcr1 &= ~dpll_cfgcr1::KDIV;
		dpll1_cfgcr1 &= ~dpll_cfgcr1::QDIV_RATIO;
		dpll1_cfgcr1 &= ~dpll_cfgcr1::QDIV_MODE;

		if (pdiv == 2) {
			dpll1_cfgcr1 |= dpll_cfgcr1::PDIV(dpll_cfgcr1::PDIV_2);
		}
		else if (pdiv == 3) {
			dpll1_cfgcr1 |= dpll_cfgcr1::PDIV(dpll_cfgcr1::PDIV_3);
		}
		else if (pdiv == 5) {
			dpll1_cfgcr1 |= dpll_cfgcr1::PDIV(dpll_cfgcr1::PDIV_5);
		}
		else if (pdiv == 7) {
			dpll1_cfgcr1 |= dpll_cfgcr1::PDIV(dpll_cfgcr1::PDIV_7);
		}
		else {
			assert(false);
		}

		if (kdiv == 1) {
			dpll1_cfgcr1 |= dpll_cfgcr1::KDIV(dpll_cfgcr1::KDIV_1);
		}
		else if (kdiv == 2) {
			dpll1_cfgcr1 |= dpll_cfgcr1::KDIV(dpll_cfgcr1::KDIV_2);
		}
		else if (kdiv == 3) {
			dpll1_cfgcr1 |= dpll_cfgcr1::KDIV(dpll_cfgcr1::KDIV_3);
		}
		else {
			assert(false);
		}

		if (qdiv != 1) {
			dpll1_cfgcr1 |= dpll_cfgcr1::QDIV_MODE(true);
			dpll1_cfgcr1 |= dpll_cfgcr1::QDIV_RATIO(qdiv);
		}

		space.store(regs::DPLL1_CFGCR1, dpll1_cfgcr1);

		space.load(regs::DPLL1_CFGCR0);

		// todo configure voltage and cd clock
	}

	dpll1_enable = space.load(regs::DPLL1_ENABLE);
	dpll1_enable |= dpll_enable::PLL_ENABLE(true);
	space.store(regs::DPLL1_ENABLE, dpll1_enable);
	while (!(space.load(regs::DPLL1_ENABLE) & dpll_enable::PLL_LOCK));

	auto dpclka_cfgcr0 = space.load(regs::DPCLKA_CFGCR0);
	if (hdmi_ddi_port == 1) {
		dpclka_cfgcr0 |= dpclka_cfgcr0::DDIB_CLOCK_SELECT(dpclka_cfgcr0::DPLL1);
		space.store(regs::DPCLKA_CFGCR0, dpclka_cfgcr0);
		dpclka_cfgcr0 &= ~dpclka_cfgcr0::DDIB_CLOCK_OFF;
		space.store(regs::DPCLKA_CFGCR0, dpclka_cfgcr0);

		auto pwr_well_ctl_ddi = space.load(regs::PWR_WELL_CTL_DDI2);
		pwr_well_ctl_ddi |= pwr_well_ctl_ddi::DDI_B_IO_POWER_REQ(true);
		space.store(regs::PWR_WELL_CTL_DDI2, pwr_well_ctl_ddi);
		while (!(space.load(regs::PWR_WELL_CTL_DDI2) & pwr_well_ctl_ddi::DDI_B_IO_POWER_STATE));

		assert(!(space.load(regs::TRANS_CONF_B) & trans_conf::TRANSCODER_STATE));

		auto trans_clk_sel_b = space.load(regs::TRANS_CLK_SEL_B);
		trans_clk_sel_b &= ~trans_clk_sel::TRANS_CLOCK_SELECT;
		trans_clk_sel_b |= trans_clk_sel::TRANS_CLOCK_SELECT(trans_clk_sel::DDIB);
		space.store(regs::TRANS_CLK_SEL_B, trans_clk_sel_b);
	}
	else {
		assert(!"unimplemented hdmi ddi port");
	}

	u16 h_total = timing_desc->h_active_pixels() + timing_desc->h_blank_pixels();
	u16 v_total = timing_desc->v_active_lines() + timing_desc->v_blank_lines();

	auto trans_hblank_b = space.load(regs::TRANS_HBLANK_B);
	trans_hblank_b &= ~trans_hblank::HBLANK_START;
	trans_hblank_b &= ~trans_hblank::HBLANK_END;
	trans_hblank_b |= trans_hblank::HBLANK_START(timing_desc->h_active_pixels() - 1);
	trans_hblank_b |= trans_hblank::HBLANK_END(h_total - 1);
	space.store(regs::TRANS_HBLANK_B, trans_hblank_b);

	auto trans_hsync_b = space.load(regs::TRANS_HSYNC_B);
	trans_hsync_b &= ~trans_hsync::HSYNC_START;
	trans_hsync_b &= ~trans_hsync::HSYNC_END;
	trans_hsync_b |= trans_hsync::HSYNC_START(
		timing_desc->h_active_pixels() +
		timing_desc->h_sync_offset_pixels() - 1);
	trans_hsync_b |= trans_hsync::HSYNC_END(
		timing_desc->h_active_pixels() +
		timing_desc->h_sync_offset_pixels() +
		timing_desc->h_sync_pulse_width_pixels() - 1);
	space.store(regs::TRANS_HSYNC_B, trans_hsync_b);

	auto trans_htotal_b = space.load(regs::TRANS_HTOTAL_B);
	trans_htotal_b &= ~trans_htotal::HACTIVE;
	trans_htotal_b &= ~trans_htotal::HTOTAL;
	trans_htotal_b |= trans_htotal::HACTIVE(timing_desc->h_active_pixels() - 1);
	trans_htotal_b |= trans_htotal::HTOTAL(h_total - 1);
	space.store(regs::TRANS_HTOTAL_B, trans_htotal_b);

	auto trans_mult_b = space.load(regs::TRANS_MULT_B);
	trans_mult_b &= ~trans_mult::MULTIPLIER;
	trans_mult_b |= trans_mult::MULTIPLIER(trans_mult::X1);
	space.store(regs::TRANS_MULT_B, trans_mult_b);

	auto trans_vblank_b = space.load(regs::TRANS_VBLANK_B);
	trans_vblank_b &= ~trans_vblank::VBLANK_START;
	trans_vblank_b &= ~trans_vblank::VBLANK_END;
	trans_vblank_b |= trans_vblank::VBLANK_START(timing_desc->v_active_lines() - 1);
	trans_vblank_b |= trans_vblank::VBLANK_END(v_total - 1);
	space.store(regs::TRANS_VBLANK_B, trans_vblank_b);

	auto trans_vsync_b = space.load(regs::TRANS_VSYNC_B);
	trans_vsync_b &= ~trans_vsync::VSYNC_START;
	trans_vsync_b &= ~trans_vsync::VSYNC_END;
	trans_vsync_b |= trans_vsync::VSYNC_START(
		timing_desc->v_active_lines() +
		timing_desc->v_sync_offset_lines() - 1);
	trans_vsync_b |= trans_vsync::VSYNC_END(
		timing_desc->v_active_lines() +
		timing_desc->v_sync_offset_lines() +
		timing_desc->v_sync_pulse_width_lines() - 1);
	space.store(regs::TRANS_VSYNC_B, trans_vsync_b);

	auto trans_vsyncshift_b = space.load(regs::TRANS_VSYNCSHIFT_B);
	trans_vsyncshift_b &= ~trans_vsyncshift::SECOND_FIELD_VSYNC_SHIFT;
	space.store(regs::TRANS_VSYNCSHIFT_B, trans_vsyncshift_b);

	auto trans_vtotal_b = space.load(regs::TRANS_VTOTAL_B);
	trans_vtotal_b &= ~trans_vtotal::VACTIVE;
	trans_vtotal_b &= ~trans_vtotal::VTOTAL;
	trans_vtotal_b |= trans_vtotal::VACTIVE(timing_desc->v_active_lines() - 1);
	trans_vtotal_b |= trans_vtotal::VTOTAL(v_total - 1);
	space.store(regs::TRANS_VTOTAL_B, trans_vtotal_b);

	auto trans_ddi_ctl_b = space.load(regs::TRANS_DDI_FUNC_CTL_B);
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::HDMI_SCRAMBLING_ENABLE;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::PORT_WIDTH_SELECT;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::HIGH_TMDS_CHAR_RATE;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::HDMI_SCRAMBLER_RESET_FREQ;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::HDMI_SCRAMBLER_CTS_ENABLE;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::SYNC_POLARITY;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::BITS_PER_COLOR;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::TRANS_DDI_MODE_SELECT;
	trans_ddi_ctl_b &= ~trans_ddi_func_ctl::DDI_SELECT;
	trans_ddi_ctl_b |= trans_ddi_func_ctl::PORT_WIDTH_SELECT(trans_ddi_func_ctl::WIDTH_4);

	assert((timing_desc->features >> 3 & 0b11) == 0b11);
	// vsync positive
	if (timing_desc->features & 1 << 2) {
		// hsync positive
		if (timing_desc->features & 1 << 1) {
			trans_ddi_ctl_b |= trans_ddi_func_ctl::SYNC_POLARITY(trans_ddi_func_ctl::SYNC_HIGH);
		}
		// hsync negative
		else {
			trans_ddi_ctl_b |= trans_ddi_func_ctl::SYNC_POLARITY(trans_ddi_func_ctl::SYNC_VS_HIGH_HS_LOW);
		}
	}
	// vsync negative
	else {
		// hsync positive
		if (timing_desc->features & 1 << 1) {
			trans_ddi_ctl_b |= trans_ddi_func_ctl::SYNC_POLARITY(trans_ddi_func_ctl::SYNC_VS_LOW_HS_HIGH);
		}
		// hsync negative
		else {
			trans_ddi_ctl_b |= trans_ddi_func_ctl::SYNC_POLARITY(trans_ddi_func_ctl::SYNC_LOW);
		}
	}

	u8 edid_bpp = edid_bpc * 3;

	u8 trans_bpc = 0;
	if (edid_bpc == 6) {
		trans_bpc = trans_ddi_func_ctl::BPC6;
	}
	else if (edid_bpc == 8) {
		trans_bpc = trans_ddi_func_ctl::BPC8;
	}
	else if (edid_bpc == 10) {
		trans_bpc = trans_ddi_func_ctl::BPC10;
	}
	else if (edid_bpc == 12) {
		trans_bpc = trans_ddi_func_ctl::BPC12;
	}
	else {
		assert(false);
	}

	trans_ddi_ctl_b |= trans_ddi_func_ctl::BITS_PER_COLOR(trans_bpc);
	trans_ddi_ctl_b |= trans_ddi_func_ctl::TRANS_DDI_MODE_SELECT(trans_ddi_func_ctl::MODE_HDMI);
	trans_ddi_ctl_b |= trans_ddi_func_ctl::DDI_SELECT(trans_ddi_func_ctl::DDIB);
	trans_ddi_ctl_b |= trans_ddi_func_ctl::TRANS_DDI_FUNC_ENABLE(true);
	space.store(regs::TRANS_DDI_FUNC_CTL_B, trans_ddi_ctl_b);

	auto trans_conf_b = space.load(regs::TRANS_CONF_B);
	trans_conf_b &= ~trans_conf::INTERLACED_MODE;
	trans_conf_b |= trans_conf::TRANSCODER_ENABLE(true);
	trans_conf_b |= trans_conf::INTERLACED_MODE(trans_conf::INTERLACED_PF_PD);
	space.store(regs::TRANS_CONF_B, trans_conf_b);

	auto port_pcs_dw1_b = space.load(regs::PORT_PCS_DW1_LN0_B);
	port_pcs_dw1_b &= ~port_pcs_dw1::CMN_KEEPER_ENABLE;
	space.store(regs::PORT_PCS_DW1_GRP_B, port_pcs_dw1_b);

	auto port_tx_dw4_ln0_b = space.load(regs::PORT_TX_DW4_LN0_B);
	port_tx_dw4_ln0_b &= ~port_tx_dw4::LOADGEN_SELECT;
	space.store(regs::PORT_TX_DW4_LN0_B, port_tx_dw4_ln0_b);
	auto port_tx_dw4_ln1_b = space.load(regs::PORT_TX_DW4_LN1_B);
	port_tx_dw4_ln1_b |= port_tx_dw4::LOADGEN_SELECT(true);
	space.store(regs::PORT_TX_DW4_LN1_B, port_tx_dw4_ln1_b);
	auto port_tx_dw4_ln2_b = space.load(regs::PORT_TX_DW4_LN2_B);
	port_tx_dw4_ln2_b |= port_tx_dw4::LOADGEN_SELECT(true);
	space.store(regs::PORT_TX_DW4_LN2_B, port_tx_dw4_ln2_b);
	auto port_tx_dw4_ln3_b = space.load(regs::PORT_TX_DW4_LN3_B);
	port_tx_dw4_ln3_b |= port_tx_dw4::LOADGEN_SELECT(true);
	space.store(regs::PORT_TX_DW4_LN3_B, port_tx_dw4_ln3_b);

	auto port_cl_dw5_b = space.load(regs::PORT_CL_DW5_B);
	port_cl_dw5_b &= ~port_cl_dw5::SUS_CLOCK_CONFIG;
	port_cl_dw5_b |= port_cl_dw5::SUS_CLOCK_CONFIG(0b11);

	auto port_tx_dw5_b = space.load(regs::PORT_TX_DW5_LN0_B);
	port_tx_dw5_b &= ~port_tx_dw5::TX_TRAINING_ENABLE;
	space.store(regs::PORT_TX_DW5_GRP_B, port_tx_dw5_b);

	port_tx_dw5_b &= ~port_tx_dw5::SCALING_MODE_SEL;
	port_tx_dw5_b &= ~port_tx_dw5::RTERM_SELECT;
	port_tx_dw5_b &= ~port_tx_dw5::CURSOR_PROGRAM;
	port_tx_dw5_b &= ~port_tx_dw5::COEFF_POLARITY;
	port_tx_dw5_b |= port_tx_dw5::SCALING_MODE_SEL(0b10);
	port_tx_dw5_b |= port_tx_dw5::RTERM_SELECT(0b110);
	port_tx_dw5_b |= port_tx_dw5::DISABLE_3TAP(true);

	// hdmi specific
	port_tx_dw5_b &= ~port_tx_dw5::DISABLE_2TAP;

	space.store(regs::PORT_TX_DW5_GRP_B, port_tx_dw5_b);

	auto port_tx_dw2_b = space.load(regs::PORT_TX_DW2_LN0_B);
	port_tx_dw2_b &= ~port_tx_dw2::RCOMP_SCALAR;
	port_tx_dw2_b |= port_tx_dw2::RCOMP_SCALAR(0x98);

	auto& hdmi_level = tgl_trans_hdmi[hdmi_level_shifter_value];

	// hdmi specific
	port_tx_dw2_b &= ~port_tx_dw2::SWING_SEL_LOWER;
	port_tx_dw2_b &= ~port_tx_dw2::SWING_SEL_UPPER;
	port_tx_dw2_b |= port_tx_dw2::SWING_SEL_LOWER(hdmi_level.swing_sel_dw2);
	port_tx_dw2_b |= port_tx_dw2::SWING_SEL_UPPER(hdmi_level.swing_sel_dw2 >> 3);

	space.store(regs::PORT_TX_DW2_GRP_B, port_tx_dw2_b);

	port_tx_dw4_ln0_b = space.load(regs::PORT_TX_DW4_LN0_B);
	port_tx_dw4_ln0_b &= ~port_tx_dw4::POST_CURSOR2;

	// hdmi specific
	port_tx_dw4_ln0_b &= ~port_tx_dw4::CURSOR_COEFF;
	port_tx_dw4_ln0_b &= ~port_tx_dw4::POST_CURSOR1;
	port_tx_dw4_ln0_b |= port_tx_dw4::CURSOR_COEFF(hdmi_level.cursor_coeff_dw4);
	port_tx_dw4_ln0_b |= port_tx_dw4::POST_CURSOR1(hdmi_level.post_cursor1_dw4);

	space.store(regs::PORT_TX_DW4_LN0_B, port_tx_dw4_ln0_b);

	port_tx_dw4_ln1_b = space.load(regs::PORT_TX_DW4_LN1_B);
	port_tx_dw4_ln1_b &= ~port_tx_dw4::POST_CURSOR2;

	// hdmi specific
	port_tx_dw4_ln1_b &= ~port_tx_dw4::CURSOR_COEFF;
	port_tx_dw4_ln1_b &= ~port_tx_dw4::POST_CURSOR1;
	port_tx_dw4_ln1_b |= port_tx_dw4::CURSOR_COEFF(hdmi_level.cursor_coeff_dw4);
	port_tx_dw4_ln1_b |= port_tx_dw4::POST_CURSOR1(hdmi_level.post_cursor1_dw4);

	space.store(regs::PORT_TX_DW4_LN1_B, port_tx_dw4_ln1_b);

	port_tx_dw4_ln2_b = space.load(regs::PORT_TX_DW4_LN2_B);
	port_tx_dw4_ln2_b &= ~port_tx_dw4::POST_CURSOR2;

	// hdmi specific
	port_tx_dw4_ln2_b &= ~port_tx_dw4::CURSOR_COEFF;
	port_tx_dw4_ln2_b &= ~port_tx_dw4::POST_CURSOR1;
	port_tx_dw4_ln2_b |= port_tx_dw4::CURSOR_COEFF(hdmi_level.cursor_coeff_dw4);
	port_tx_dw4_ln2_b |= port_tx_dw4::POST_CURSOR1(hdmi_level.post_cursor1_dw4);

	space.store(regs::PORT_TX_DW4_LN2_B, port_tx_dw4_ln2_b);

	port_tx_dw4_ln3_b = space.load(regs::PORT_TX_DW4_LN3_B);
	port_tx_dw4_ln3_b &= ~port_tx_dw4::POST_CURSOR2;

	// hdmi specific
	port_tx_dw4_ln3_b &= ~port_tx_dw4::CURSOR_COEFF;
	port_tx_dw4_ln3_b &= ~port_tx_dw4::POST_CURSOR1;
	port_tx_dw4_ln3_b |= port_tx_dw4::CURSOR_COEFF(hdmi_level.cursor_coeff_dw4);
	port_tx_dw4_ln3_b |= port_tx_dw4::POST_CURSOR1(hdmi_level.post_cursor1_dw4);

	space.store(regs::PORT_TX_DW4_LN3_B, port_tx_dw4_ln3_b);

	// hdmi specific
	auto port_tx_dw7_b = space.load(regs::PORT_TX_DW7_LN0_B);
	port_tx_dw7_b &= ~port_tx_dw7::N_SCALAR;
	port_tx_dw7_b |= port_tx_dw7::N_SCALAR(hdmi_level.n_scalar_dw7);
	space.store(regs::PORT_TX_DW7_GRP_B, port_tx_dw7_b);

	port_tx_dw5_b = space.load(regs::PORT_TX_DW5_LN0_B);
	port_tx_dw5_b |= port_tx_dw5::TX_TRAINING_ENABLE(true);
	space.store(regs::PORT_TX_DW5_GRP_B, port_tx_dw5_b);

	auto port_cl_dw10_b = space.load(regs::PORT_CL_DW10_B);
	port_cl_dw10_b &= ~port_cl_dw10::STATIC_POWER_DOWN;
	space.store(regs::PORT_CL_DW10_B, port_cl_dw10_b);

	auto pipe_srcsz_b = space.load(regs::PIPE_SRCSZ_B);
	pipe_srcsz_b &= ~pipe_srcsz::HEIGHT;
	pipe_srcsz_b &= ~pipe_srcsz::WIDTH;
	pipe_srcsz_b |= pipe_srcsz::HEIGHT(timing_desc->v_active_lines() - 1);
	pipe_srcsz_b |= pipe_srcsz::WIDTH(timing_desc->h_active_pixels() - 1);
	space.store(regs::PIPE_SRCSZ_B, pipe_srcsz_b);

	auto pipe_bottom_color_b = space.load(regs::PIPE_BOTTOM_COLOR_B);
	pipe_bottom_color_b &= ~pipe_bottom_color::B_BOTTOM_COLOR;
	pipe_bottom_color_b &= ~pipe_bottom_color::G_BOTTOM_COLOR;
	pipe_bottom_color_b &= ~pipe_bottom_color::R_BOTTOM_COLOR;
	pipe_bottom_color_b |= pipe_bottom_color::B_BOTTOM_COLOR(0xFF);
	space.store(regs::PIPE_BOTTOM_COLOR_B, pipe_bottom_color_b);

	auto plane_size_1_b = space.load(regs::PLANE_SIZE_1_B);
	plane_size_1_b &= ~plane_size::WIDTH;
	plane_size_1_b &= ~plane_size::HEIGHT;
	plane_size_1_b |= plane_size::WIDTH(timing_desc->h_active_pixels() - 1);
	plane_size_1_b |= plane_size::HEIGHT(timing_desc->v_active_lines() - 1);
	space.store(regs::PLANE_SIZE_1_B, plane_size_1_b);

	// stride in 64-byte blocks with linear
	auto plane_stride_1_b = space.load(regs::PLANE_STRIDE_1_B);
	plane_stride_1_b &= ~plane_stride::STRIDE;
	plane_stride_1_b |= plane_stride::STRIDE(space.load(regs::PLANE_STRIDE_1_A) & plane_stride::STRIDE);
	space.store(regs::PLANE_STRIDE_1_B, plane_stride_1_b);

	auto ddi_buf_ctl_b = space.load(regs::DDI_BUF_CTL_B);
	ddi_buf_ctl_b &= ~ddi_buf_ctl::PORT_REVERSAL;
	ddi_buf_ctl_b &= ~ddi_buf_ctl::OVERRIDE_TRAINING_ENABLE;
	ddi_buf_ctl_b |= ddi_buf_ctl::DDI_BUFFER_ENABLE(true);
	space.store(regs::DDI_BUF_CTL_B, ddi_buf_ctl_b);
	while (space.load(regs::DDI_BUF_CTL_B) & ddi_buf_ctl::DDI_IDLE_STATUS);

	// modify plane 1A buffer location
	auto plane_buf_cfg_1_a = space.load(regs::PLANE_BUF_CFG_1_A);
	plane_buf_cfg_1_a &= ~plane_buf_cfg::BUFFER_END;
	plane_buf_cfg_1_a |= plane_buf_cfg::BUFFER_END(511);
	space.store(regs::PLANE_BUF_CFG_1_A, plane_buf_cfg_1_a);

	// force update plane 1A
	space.store(regs::PLANE_SURF_1_A, space.load(regs::PLANE_SURF_1_A));

	auto plane_buf_cfg_1_b = space.load(regs::PLANE_BUF_CFG_1_B);
	plane_buf_cfg_1_b &= ~plane_buf_cfg::BUFFER_START;
	plane_buf_cfg_1_b &= ~plane_buf_cfg::BUFFER_END;
	plane_buf_cfg_1_b |= plane_buf_cfg::BUFFER_START(512);
	plane_buf_cfg_1_b |= plane_buf_cfg::BUFFER_END(1023);
	space.store(regs::PLANE_BUF_CFG_1_B, plane_buf_cfg_1_b);

	auto plane_ctl_1_b = space.load(regs::PLANE_CTL_1_B);
	plane_ctl_1_b &= ~plane_ctl::TILED_SURFACE;
	plane_ctl_1_b &= ~plane_ctl::RGB_COLOR_ORDER;
	plane_ctl_1_b &= ~plane_ctl::SRC_PIXEL_FMT;
	plane_ctl_1_b |= plane_ctl::TILED_SURFACE(plane_ctl::TILE_LINEAR);
	plane_ctl_1_b |= plane_ctl::RGB_COLOR_ORDER(plane_ctl::ORDER_RGBX);
	plane_ctl_1_b |= plane_ctl::SRC_PIXEL_FMT(plane_ctl::FMT_RGB8888);
	plane_ctl_1_b |= plane_ctl::PLANE_ENABLE(true);
	space.store(regs::PLANE_CTL_1_B, plane_ctl_1_b);

	/*auto pipe_imr = space.load(regs::DE_PIPE_IMR_A);
	pipe_imr &= ~de_pipe_int::VBLANK;
	space.store(regs::DE_PIPE_IMR_A, pipe_imr);

	auto pipe_ier = space.load(regs::DE_PIPE_IER_A);
	pipe_ier |= de_pipe_int::VBLANK(true);
	space.store(regs::DE_PIPE_IER_A, pipe_ier);

	// clear irr
	auto pipe_irr = space.load(regs::DE_PIPE_IRR_A);
	space.store(regs::DE_PIPE_IRR_A, pipe_irr);
	pipe_irr = space.load(regs::DE_PIPE_IRR_A);
	space.store(regs::DE_PIPE_IRR_A, pipe_irr);

	while (!(space.load(regs::DE_PIPE_IRR_A) & de_pipe_int::VBLANK));

	println("test: ", space.load(regs::DISPLAY_INT_CTL));*/

	print(Fmt::Reset);

	return InitStatus::Success;
}

static PciDriver INTEL_DRIVER {
	.init = intel_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x8086, .device = 0x9A40},
		{.vendor = 0x8086, .device = 0x9A49},
		{.vendor = 0x8086, .device = 0xA780}
	}
};

PCI_DRIVER(INTEL_DRIVER);
