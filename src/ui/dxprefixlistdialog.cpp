#include "dxprefixlistdialog.h"

#include "inwindowdialog.h"
#include "k4styles.h"

#include <QCollator>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputMethod>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScroller>
#include <QScrollerProperties>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char kDxPrefixText[] = R"DXPREFIX(
A2 Botswana
A3 Tonga
A4 Sultanate of Oman
A5 Bhutan
A6 United Arab Emirates
A7 Qatar
A8 Liberia EL
A9 Bahrain
AA-AG USA W
AH1-AH0 USA Pacific Islands KH1-KH0
AI-AK USA W
AL Alaska KL
AM-AO Spain including overseas Territories and Islands EA 6 8 9
AP-AR Pakistan
AT India VU
AX Australia and Islands
AY-AZ Argentina LU
BO Quemoy Matsu BV
BS Scarborough Reef
BV Taiwan
BV9P Pratas I.
BV9S Spratly Archipelago 9M0
BY China BA BD BG BT BZ
C2 Nauru
C3 Andorra
C4 Cyprus 5B
C5 Gambia
C6 Bahamas
C8, C9 Mozambique
CE Chile
CE0 Easter I.
CE0 San Felix and San Ambrosio Is
CE0 Juan Fernandez Is
CF-CK Canada VE
CL CM Cuba CO
CN Morocco
CO Cuba
CP Bolivia
CT1CQ-CT2 4-8 0 Portugal
CT3 CQ-CS3 CT9 Madeira Is
CU Azores
CX CV CW Uruguay
CY CZ Canada VE
CY9 St Paul Is
CY0 Sable I.
D2 D3 Angola
D4 Cape Verde
D6 Comoros
D7 Korea (Republic of) HL
DL DA-DD DF-DH Federal Republic
DJ DK DP of Germany
DS Korea (Republic of) HL
DU DV-DZ Philippines
DU Spratly Archipelago 9M0
E2 Thailand HS
E3 Eritrea
E4 Palestine
E5 Cook Islands (New Zealand)
E7 Bosnia-Herzegovina (was T9)
E6 Niue (New Zealand)
EA EB-EH1-5 7 0 Spain
EA6 EB6-EH6 Balearic Is
EA8 EB8-EH8 Canary Is
EA9 EB9-EH9 Ceuta and Melilla
EI-EJ Republic of Ireland
EK Armenia
EL Liberia
EM EN EO Ukraine UR
EP Iran
ER Moldova
ES Estonia
ET Ethiopia
EU EV EW Belarus
EX Kyrghyzstan
EY Tajikistan
EZ Turkmenistan
F France
FG Guadeloupe
FH Mayotte
FJ St Barthelemy (French St Martin) FS
FK New Caledonia
FK—-/C Chesterfield Is
FM Martinique
FO Austral Is
FO French Polynesia
FO Marquesas Is
FO8X Clipperton I.
FP St Pierre and Miquelon
FR Reunion I.
FR——/E Europa I. FR——/J
FR——/G Glorioso Is
FR——/J Juan de Nova
FR——/T Tromelin I.
FS French St Martin
FTnW Crozet Is
FTnX Kerguelen Is
FTnZ Amsterdam I. and St Paul I.
FW Wallis and Futuna Is
FY French Guiana
G GX England
GB United Kingdom G GD GI GJ GM GU GW
GD GT Isle of Man
GI GN Northern Ireland
GJ GH Jersey
GM GS Scotland
GU GP Guernsey and Dependencies
GW GC Wales
H2 Cyprus 5B
H3 Panama HP
H4 Solomon Is
H40 Temotu Province
H5 Bophuthatswana ZS
H6 H7 Nicaragua YN
H8 H9 Panama HP
HA Hungary
HB Switzerland
HB0 Liechtenstein
HC,HD Ecuador
HC8,HD8 Galapagos Is
HE Switzerland HB
HF Poland SP
HG Hungary HA
HH Haiti
HI Dominican Republic
HK HJ Colombia
HK0 Malpelo I.
HK0 HJ0 San Andres and Providencia
HL Korea (Republic of)
HP HO Panama
HR HQ Honduras
HS Thailand
HT Nicaragua YN
HU El Salvador YS
HV Vatican City
HZ Saudi Arabia
I IA-IH IK IL IN IP Italy
IR IT IV-IX
IS0 IM0 Sardinia
J2 Djibouti
J3 Grenada
J4 Greece SV
J5 Guinea-Bissau
J6 St Lucia
J7 Dominica
J8 St Vincent and the Grenadines
JA JE-JS Japan
JD 7J Minami Torishima
JD 7J Ogasawara Is
JT JU JV Mongolia
JW Svalbard
JX Jan Mayen
JY Jordan
K KA-KZ USA and US Islands W KC6xx KG4xx KH1-0 KP1-5
KC6 x x Republic of Palau
KG4 x x Guantanamo Bay
KG6 x x Guam
KH1 Baker I. and Howland I.
KH2 ( KG6 ) Guam
KH3 Johnston I.
KH4 Midway Is.
KH5 Palmyra I.
KH5J Jarvis I. KH5
KH5K Kingman Reef
KH6 7 Hawaiian Is
KH7K Kure I.
KH8 American Samoa
KH9 Wake I.
KH0 North Mariana
KL Alaska
KP1 Navassa I.
KP2 US Virgin Is
KP3 4 Puerto Rico
KP5 Desecheo I.
L2-L9 Argentina LU
LA LB LC LG LI Norway
LJ LN
LU LO-LT LV LW Argentina
LX Luxembourg
LY Lithuania
LZ Bulgaria
M MX England G
MD MT Isle of Man GD
MI MN Northern Ireland GI
MJ MH Jersey GJ
MM MS Scotland GM
MU MP Guernsey and Dependencies GU
MW MC Wales GW
N NA-NG NI-NK USA W
NM-NO NQ-NZ
NH1-NH0 US Pacific islands KH1-KH0
NL Alaska KL
NP1-NP5 US Caribbean Islands KP1-KP5
OA OB OC Peru
OD Lebanon
OE Austria
OH OF OG OI Finland
OH0 OF0 OG0 Aland Is
OJ0 OF0M OH0M Market Reef
OK OL Czech Republic
OM Slovak Republic
ON OO-OT Belgium
OX Greenland
OY Faroe Is
OU OZ Denmark
P2 Papua New Guinea
P3 Cyprus 5B
P4 Aruba
P5 Korea (Dem Peoples Rep of)
PA – PI Netherlands
PJ1 PJ2 4 9 Netherlands Antilles
PJ5 PJ6 7 8 Sint Maarten, Saba and St Eustatius
PY PP-PX Brazil
PY0F Fernando de Noronha Archipelago
PY0M Martim Vaz I. PU0T
PY0R Atol das Rocas PY0F
PY0S St Peter and St Paul Rocks
PY0T Trindade I.
PZ Suriname
R1A Antarctica
R1F Franz Josef Land
R1M Malyj Vysotskij I.
R RA RK RN RU-RZ European Russia UA
R RA RK RN RU-RZ Asiatic Russia UA9
R2 RA2 RK2 RN2 RY2 Kaliningradsk UA2
S2 Bangladesh
S4 Ciskei ZS
S5 Slovenia
S6 Singapore 9V
S7 Republic of Seychelles
S8 Transkei ZS
S9 Sao Tome and Principe
S0 Western Sahara
SM SH-SL Sweden
SP SN-SR Poland
ST Republic of the Sudan
SU Egypt
SV SX-SZ Greece
SV—/A Mount Athos
SV5 Dodecanese Is
SV9 Crete
SV0 Non-nationals in Greece or on Greek Is SV SV5 SV9
T2 Tuvalu
T30 West Kiribati
T31 Central Kiribati
T32 East Kiribati
T33 Banaba
T4 Cuba CO
T5 Somalia
T6 Afghanistan YA
T7 San Marino
T9 Bosnia-Herzegovina (now E7)
TA Turkey
TD Guatemala TG
TE Costa Rica TI
TF Iceland
TG Guatemala
TI Costa Rica
TI9 Cocos I.
TJ Cameroon
TK Corsica
TL Central African Republic
TM France including overseas Territories and Departments F
TN Congo
TO France including overseas Territories and Departments FG FJ FM FP FR FS FY
TP Council of Europe-Strasbourg F
TR Gabon
TT Chad
TU Cote d’Ivoire
TX France including overseas Territories and Departments FK FO FW
TY Benin
TZ Mali
UA U UA UE 1 3 4 6 European Russia
UA2 U UA UE 2 Kaliningrad
UA9 U UA UE 8-0 Asiatic Russia
UK U8 UJ UK7-9 UM Uzbekistan
UN UN1-0 UP UQ Kazakhstan
UR US-UZ Ukraine
V2 Antigua and Barbuda
V3 Belize
V4 Federation of St Kitts and Nevis
V5 Namibia
V6 Micronesia
V7 Marshall Is
V8 Brunei Darussalam
V9 Vendaland ZS
VE VA-VG Canada
VE0 Canadian /MM Stations
VK VI Australia
VK9C Cocos Keeling Is
VK9L Lord Howe I.
VK9M Mellish Reef
VK9N Norfolk I.
VK9W Willis Is
VK9X Christmas I.
VK0 Heard I.
VK0 Macquarie I.
VO1 VO3 5 7 9 Newfoundland VE
VO2 VO4 6 8 0 Labrador VE
VP2E Anguilla
VP2M Montserrat
VP2V British Virgin Is
VP5 Turks and Caicos Is
VP6 Pitcairn Is
VP8 Antarctica
VP8 Falkland Is
VP8 South Georgia
VP8 AZ1 5 ED0 L South Orkney Is
UnZx
VP8 South Sandwich Is
VP8 CE9 CX0 ED0 South Shetland Is
HF0 HL5 LUnZx South Shetland Is cont.
ZX0 4K1
VP9 Bermuda
VQ9 Chagos Is
VR2 Special Administrative Region of Hong Kong
VU India
VU Lakshadweep
VU Andaman Is and Nicobar Is
VX VY Canada VE
VY1 Yukon Territory VE
VY2 Prince Edward I. VE
W WA-WG WI-WK USA
WM-WO WQ-WZ
WH1-WH0 US Pacific Islands KH1-KH0
WL Alaska KL
WP1-WP5 US Caribbean Islands KP1-KP5
XE XB-XH Mexico
XF4 Revilla Gigedo Is
XJ-XO Canada VE
XQ XR Chile and Islands CE CE9 CE0
XT Burkina Faso
XU Cambodia
XV Vietnam 3W
XW Lao Peoples Democratic Republic
XX China
XX9 Macao
XY XZ Myanmar
XZ5 XZ9 Karen State XZ
YA Republic of Afghanistan
YBYC YE-YH Indonesia
YI Iraq
YJ Vanuatu
YK Syria
YL Latvia
YM TurkeyTA
YN Nicaragua
YO YP-YR Romania
YS El Salvador
YU YT Serbia (ex Yugoslavia)
YV YW-YY Venezuela
YV0 Aves I.
YZ Yugoslavia YU
Z2 Zimbabwe
Z3 North Macedonia
Z6 Kosovo
Z8 South Sudan
ZA Albania
ZB ZG Gibraltar
ZC UK Sovereign Bases on Cyprus-Akrotiri and Dhekelia
ZD7 St Helena
ZD8 Ascension I.
ZD9 Tristan da Cunha and Gough I.
ZF Cayman Islands
ZK1 South Cook Is
ZK1 Northern Cook Is
ZK2 ZK9 Niue
ZK3 Tokelau Is
ZL New Zealand
ZL7 Chatham Is
ZL8 Kermadec Is
ZL9 Auckland I. and Campbell I.
ZM New Zealand and Islands ZL ZL7 ZL8 ZL9
ZP Paraguay
ZS ZR ZU Republic of South Africa
ZS8 Prince Edward I. and Marion I.
ZV-ZZ Brazil and Islands PY PY0
1A0 Sovereign Military Order of Malta (Rome, Italy)
1C Chechnya Rep. (Russian Federation)
1P Seborga Principato (Italy)
1S Spratly Archipelago 9M0
2D Isle of Man GD
2E England G
2I Northern Ireland GI
2J Jersey GJ
2M Scotland GM
2U Guernsey and Dependencies GU
2W Wales GW
3A Monaco
3B6 Agalega Is
3B7 Cargados Carajos (St Brandon) 3B6
3B8 Mauritius
3B9 Rodriguez I.
3C Equatorial Guinea
3C0 Annobon I.
3D2 Republic of Fiji
3D2 Conway Reef
3D2 Rotuma I.
3DA0 Eswatini (Swaziland)
3E-3F Panama HP
3G Chile and Islands CE CE9 CE0
3V Tunisia
3W XV Vietnam
3X Republic of Guinea
3Y Bouvet I.
3Y Peter I Island
3Z Poland SP
4A-4C Mexico and Islands XE XF4
4D-4I Philippines DU
4J 4K Azerbaijan
4L Georgia
4M Venezuela and Islands YV YV0
4N1 6-0 Yugoslavia YU
4O Montenegro
4S Sri Lanka
4T Peru OA
4U United Nations Organization
4U1ITU 4UnITU United Nations Geneva
4U1SCO UNESCO, Paris F
4U1UN 4UnUN United Nations New York
4U1VIC United Nations Vienna OE
4U1WB World Bank Washington D.C. W
4V Haiti HH
4W East Timor
4X 4Z Israel
5A Libya
5B Cyprus
5C Morocco CN
5H Tanzania
5J 5K Colombia and Islands HK HK0
5L Liberia EL
5N Nigeria
5P Denmark OZ
5R Madagascar
5T Mauritania
5U Niger
5V Togo
5W Western Samoa
5X Uganda
5Y 5Z Kenya
6C Syria YK
6D-6J Mexico and Islands XE-XF4
6K 6L Republic of Korea HL
6O Somalia T5
6P Pakistan AP
6T 6U Republic of the Sudan
6W 6V Senegal
6Y Jamaica
7J-7N Japan JA
7O Republic of Yemen
7P Lesotho
7Q Malawi
7S Sweden SM
7X 7W Algeria
7Z Saudi Arabia HZ
8A 8B 8E 8I Indonesia YB
8J Japan JA
8O Botswana A2
8P Barbados
8Q Maldives
8R Guyana
8S Sweden SM
9A Croatia
9G Ghana
9H Malta
9J 9I Zambia
9K Kuwait
9L Sierra Leone
9M2 Malaya (Malaysia)
9M6 Sabah (Malaysia) 9M8
9M8 Sarawak (Malaysia)
9M0 BV9S 1S DU Spratly Archipelago
9N Nepal
9Q 9R Democratic Republic of Congo
9U Burundi
9V Singapore
9W Malaysia (including Sabah & Sarawak) 9M2 8
9X Rwanda
9Y 9Z Trinidad and Tobago
)DXPREFIX";

struct DxEntry {
    QString prefix;
    QString description;
    QString searchable;
};

QVector<DxEntry> dxEntries() {
    QVector<DxEntry> entries;
    const QStringList rows = QString::fromUtf8(kDxPrefixText).split('\n', Qt::SkipEmptyParts);
    entries.reserve(rows.size());
    for (const QString &untrimmed : rows) {
        const QString row = untrimmed.trimmed();
        const qsizetype separator = row.indexOf(' ');
        DxEntry entry;
        if (separator < 0) {
            entry.prefix = row;
        } else {
            entry.prefix = row.left(separator);
            entry.description = row.mid(separator + 1).trimmed();
        }
        entry.searchable = row.toCaseFolded();
        entries.push_back(entry);
    }

    QCollator collator(QLocale::English);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::stable_sort(entries.begin(), entries.end(), [&collator](const DxEntry &left, const DxEntry &right) {
        const bool leftNumbered = !left.prefix.isEmpty() && left.prefix.front().isDigit();
        const bool rightNumbered = !right.prefix.isEmpty() && right.prefix.front().isDigit();
        if (leftNumbered != rightNumbered)
            return leftNumbered;
        const int prefixOrder = collator.compare(left.prefix, right.prefix);
        if (prefixOrder != 0)
            return prefixOrder < 0;
        return collator.compare(left.description, right.description) < 0;
    });
    return entries;
}

QString actionButtonStyle(int fontSize) {
    return QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 5px;"
        " font-size: %5px; font-weight: 600; padding: 2px 8px; }"
        "QPushButton:pressed { background: %3; color: %4; }")
        .arg(K4Styles::Colors::PopupBackground,
             K4Styles::Colors::TextWhite,
             K4Styles::Colors::AccentAmber,
             K4Styles::Colors::Background)
        .arg(fontSize);
}

} // namespace

void showDxPrefixList(QWidget *parent) {
    if (!parent)
        return;

    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *outer = new QVBoxLayout(panel);
    outer->setContentsMargins(12, 9, 12, 9);
    outer->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("DX PREFIX LIST"), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 700;")
                             .arg(K4Styles::Colors::AccentAmber));
    outer->addWidget(title);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);
    auto *search = new QLineEdit(panel);
    search->setPlaceholderText(QStringLiteral("Search prefix or country"));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(300);
    search->setMaximumWidth(440);
    search->setMinimumHeight(29);
    search->setMaximumHeight(31);
    search->setInputMethodHints(Qt::ImhNoPredictiveText | Qt::ImhPreferUppercase);
    search->setStyleSheet(QString(
        "QLineEdit { background: #111719; color: %1; border: 2px solid %2; border-radius: 5px;"
        " font-size: 11px; padding: 2px 7px; selection-background-color: %2; selection-color: #101314; }")
                              .arg(K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder));
    searchRow->addWidget(search);
    searchRow->addStretch(1);

    auto *previous = new QPushButton(QString::fromUtf8("▲"), panel);
    auto *next = new QPushButton(QString::fromUtf8("▼"), panel);
    for (QPushButton *button : {previous, next}) {
        button->setFixedSize(36, 31);
        button->setStyleSheet(actionButtonStyle(11));
    }
    previous->setAccessibleName(QStringLiteral("Previous search match"));
    next->setAccessibleName(QStringLiteral("Next search match"));
    searchRow->addWidget(previous);
    searchRow->addWidget(next);
    outer->addLayout(searchRow);

    auto *status = new QLabel(panel);
    status->setStyleSheet(QString("color: %1; font-size: 10px;").arg(K4Styles::Colors::TextGray));
    outer->addWidget(status);

    auto *list = new QTreeWidget(panel);
    list->setColumnCount(2);
    list->setHeaderLabels({QStringLiteral("PREFIX"), QStringLiteral("COUNTRY / AREA / CROSS-REFERENCE")});
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setUniformRowHeights(true);
    list->setAlternatingRowColors(true);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    list->setColumnWidth(0, 125);
    list->setStyleSheet(QString(
        "QTreeWidget { background: #0d1112; color: %1; border: 1px solid %2; font-size: 11px;"
        " alternate-background-color: #151b1d; outline: none; }"
        "QTreeWidget::item { min-height: 24px; padding: 1px 5px; border-bottom: 1px solid #263034; }"
        "QTreeWidget::item:selected { background: %3; color: #101314; }"
        "QHeaderView::section { background: #20282b; color: %3; border: 0; border-right: 1px solid %2;"
        " padding: 3px 6px; font-size: 10px; font-weight: 700; }")
                            .arg(K4Styles::Colors::TextWhite,
                                 K4Styles::Colors::DialogBorder,
                                 K4Styles::Colors::AccentAmber));

    const QVector<DxEntry> entries = dxEntries();
    QVector<QTreeWidgetItem *> items;
    items.reserve(entries.size());
    for (const DxEntry &entry : entries) {
        auto *item = new QTreeWidgetItem(list, {entry.prefix, entry.description});
        item->setData(0, Qt::UserRole, entry.searchable);
        item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
        item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
        items.push_back(item);
    }
    outer->addWidget(list, 1);

    QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
    if (QScroller *scroller = QScroller::scroller(list->viewport())) {
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
        properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                                   QScrollerProperties::OvershootWhenScrollable);
        properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                                   QScrollerProperties::OvershootAlwaysOff);
        scroller->setScrollerProperties(properties);
    }

    auto *close = new QPushButton(QStringLiteral("BACK TO RADIO"), panel);
    close->setMinimumHeight(32);
    close->setMaximumHeight(34);
    close->setStyleSheet(actionButtonStyle(11));
    outer->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, &dialog, &InWindowDialog::reject);

    QVector<int> matches;
    int matchIndex = -1;
    const auto showMatch = [&]() {
        if (matches.isEmpty() || matchIndex < 0) {
            list->clearSelection();
            return;
        }
        matchIndex = (matchIndex % matches.size() + matches.size()) % matches.size();
        QTreeWidgetItem *item = items.at(matches.at(matchIndex));
        list->setCurrentItem(item);
        list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        status->setText(QStringLiteral("Match %1 of %2 • use ▲/▼ for other matches")
                            .arg(matchIndex + 1)
                            .arg(matches.size()));
    };

    const auto updateSearch = [&](const QString &text) {
        matches.clear();
        matchIndex = -1;
        const QString query = text.trimmed().toCaseFolded();
        if (query.isEmpty()) {
            list->clearSelection();
            list->scrollToTop();
            status->setText(QStringLiteral("%1 entries • drag the list or search by prefix or country")
                                .arg(entries.size()));
            return;
        }

        // Exact prefix starts are the most useful matches. Country and
        // cross-reference matches follow, matching the K4 manual's search scope.
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).prefix.toCaseFolded().startsWith(query))
                matches.push_back(i);
        }
        for (int i = 0; i < entries.size(); ++i) {
            if (!matches.contains(i) && entries.at(i).searchable.contains(query))
                matches.push_back(i);
        }

        if (matches.isEmpty()) {
            list->clearSelection();
            status->setText(QStringLiteral("No matches for “%1”").arg(text.trimmed()));
            return;
        }
        matchIndex = 0;
        showMatch();
    };

    QObject::connect(search, &QLineEdit::textChanged, panel, updateSearch);
    QObject::connect(search, &QLineEdit::returnPressed, panel, [&]() {
        // Commit the query and leave the first/best match selected. Keeping
        // the line edit focused causes Android to re-open its IME immediately
        // after the keyboard's Enter action dismisses it.
        updateSearch(search->text());
        search->clearFocus();
        list->setFocus(Qt::OtherFocusReason);
        if (QInputMethod *inputMethod = QGuiApplication::inputMethod())
            inputMethod->hide();
    });
    QObject::connect(previous, &QPushButton::clicked, panel, [&]() {
        if (!matches.isEmpty()) {
            --matchIndex;
            showMatch();
        }
    });
    QObject::connect(next, &QPushButton::clicked, panel, [&]() {
        if (!matches.isEmpty()) {
            ++matchIndex;
            showMatch();
        }
    });

    updateSearch(QString());
    const QSize available = parent->size() - QSize(20, 16);
    dialog.setPanelSize(QSize(qMin(1050, qMax(280, available.width())),
                              qMin(720, qMax(300, available.height()))));
    dialog.exec();
}
