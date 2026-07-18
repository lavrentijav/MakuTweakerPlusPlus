static const uint8_t kBHostA[] = {167,221,131,239,138,1,124,19,121,37,92,55,109,73,33,91,13,109,18,127,237,145,183,163,216,181,223,199,183,217,133,235,157,253,104,15,127,33,7,51,72,69};
static constexpr uint8_t kSHostA = 0xD4;
static constexpr bool kWHostA = true;

static const uint8_t kBHostB[] = {190,225,222,243,155,5,120,23,87,41,68,59,42,77,96,95,9,113,87,131,249,149,253,167,158,185,163,203,250,221,148,239,141,1,100,19,50,37,79,55,48,73,34,91};
static constexpr uint8_t kSHostB = 0xD8;
static constexpr bool kWHostB = true;

static const uint8_t kBPath[] = {206,234,148,252,42,14,116,32,70,50,87,68,33,86,58,104,18,122,247,140};
static constexpr uint8_t kSPath = 0xE1;
static constexpr bool kWPath = true;

static const uint8_t kBSalt[] = {234,223,190,153,87,109,84,41,52,19,255,244,215,167,154,80,123,91,54,3,32,239,194,202,170,146,162,71,74,46,45,47,181,174,135,250};
static constexpr uint8_t kSSalt = 0xA7;
static constexpr bool kWSalt = false;

static const uint8_t kBTid[] = {245,228,167,191,68,97,12,107,60,184,220,155};
static constexpr uint8_t kSTid = 0xB2;
static constexpr bool kWTid = false;

static const uint8_t kBHdr[] = {144,242,183,125,84,22,1,0,231,249,207,177,169,129,111,27,24};
static constexpr uint8_t kSHdr = 0xC8;
static constexpr bool kWHdr = false;

static const uint8_t kBPost[] = {163,252,74,14,68,32,125,50};
static constexpr uint8_t kSPost = 0xF3;
static constexpr bool kWPost = true;

static const uint8_t kBKv[] = {103};
static constexpr uint8_t kSKv = 0x11;
static constexpr bool kWKv = false;

static const uint8_t kBKtid[] = {86,80,52};
static constexpr uint8_t kSKtid = 0x22;
static constexpr bool kWKtid = false;

static const uint8_t kBKcid[] = {80,35,5};
static constexpr uint8_t kSKcid = 0x33;
static constexpr bool kWKcid = false;

static const uint8_t kBKen[] = {33,53};
static constexpr uint8_t kSKen = 0x44;
static constexpr bool kWKen = false;

static const uint8_t kBKepn[] = {48,28,237,180,212,166,184,151,106,65,86,55,7,244,200,218,172,177,150,85,76,75,42,5};
static constexpr uint8_t kSKepn = 0x55;
static constexpr bool kWKepn = false;

static const uint8_t kBKver[] = {83};
static constexpr uint8_t kSKver = 0x61;
static constexpr bool kWKver = false;

static const uint8_t kBCidFn[] = {5,17,253,206,172,173,175,110,122,27,56,27,14};
static constexpr uint8_t kSCidFn = 0x66;
static constexpr bool kWCidFn = false;

static const uint8_t kBEpLang[] = {20,248,177,215,189,148,164,126,72,46,48,27,228,251,214};
static constexpr uint8_t kSEpLang = 0x71;
static constexpr bool kWEpLang = false;

static const uint8_t kBEpScr[] = {23,249,142,196,173,151,153,118,68,30,54,14,235,248};
static constexpr uint8_t kSEpScr = 0x72;
static constexpr bool kWEpScr = false;

static const uint8_t kBEpCpu[] = {22,250,143,219,191,147,162,122,74,47,60};
static constexpr uint8_t kSEpCpu = 0x73;
static constexpr bool kWEpCpu = false;

static const uint8_t kBEpSt[] = {17,251,140,202,179,136,140,112,115,55,35,1,237};
static constexpr uint8_t kSEpSt = 0x74;
static constexpr bool kWEpSt = false;

static const uint8_t kBEpSc[] = {16,252,141,201,178,135,141,115};
static constexpr uint8_t kSEpSc = 0x75;
static constexpr bool kWEpSc = false;

static const uint8_t kBEvBench[] = {227,253,193,165,181,153,106,80,82,15,21,27,230,217,175,174};
static constexpr uint8_t kSEvBench = 0x81;
static constexpr bool kWEvBench = false;

static const uint8_t kBEvLaunch[] = {227,233,192,152,178,148,121,77,89,57};
static constexpr uint8_t kSEvLaunch = 0x82;
static constexpr bool kWEvLaunch = false;

static const uint8_t kBEvLaunch30[] = {226,234,193,151,179,151,120,74,88,58,54,179,167,221,160,191};
static constexpr uint8_t kSEvLaunch30 = 0x83;
static constexpr bool kWEvLaunch30 = false;

static const uint8_t kBEvScreen[] = {247,248,192,172,133,153,81,83,85,54,29};
static constexpr uint8_t kSEvScreen = 0x84;
static constexpr bool kWEvScreen = false;

static const uint8_t kBUa1[] = {19,103,31,121,248,139,253,157,202,175,212,193,171,211,243,229,219,247,46,9,34,27,4,45,30,63,31,81,51,99,2,117,26,135,255,153,213,171,199,189,230,207,150,225,190,243,220,5,63,23,16,41,28,59,116,77,109,95,72,113,45,131,229,149,240,167,134,185,246,203,239,221,198,239,128,1,60,19,40,37,7,55,96,73,19,91,20,109,6,127,228,145,255,163,251,181,219,199,178,217,169,235,157,253,114,15,55,33,31,51,15,69,121,87,78,105,65,123,178,141,182,159};
static constexpr uint8_t kSUa1 = 0x5E;
static constexpr bool kWUa1 = true;

static const uint8_t kBUa2[] = {71,120,202,138,219,156,241,174,250,192,133,210,247,228,205,246,147,8,120,26,72,44,80,62,103,80,30,98,14,116,30,134,228,152,206,170,154,188,229,206,148,224,129,242,137,4,98,22,114,40,84,58,108,76,100,94,85,112,73,130,165,148,173,166,129,184,241,202,253,220,213,238,215,0,90,18,122,36,75,54,94,72,35,90,10,108,90,126,178,144,170,162,156,180,147,198,252,216,215,234};
static constexpr uint8_t kSUa2 = 0x6F;
static constexpr bool kWUa2 = true;
