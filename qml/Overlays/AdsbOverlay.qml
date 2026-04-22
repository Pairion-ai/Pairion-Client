/**
 * @file AdsbOverlay.qml
 * @brief ADS-B aircraft overlay — live aircraft icons and callout boxes.
 *
 * Displays aircraft positions received via SceneDataPush (modelId="adsb") on top of
 * the active background. Calls background.latLonToScreen(lat, lon) to map geographic
 * coordinates to screen pixels; designed for use with VfrBackground.
 *
 * Aircraft rendering:
 *   - Airborne GA aircraft: arrow silhouette (cyan) rotated to heading.
 *   - Airborne airline aircraft: arrow silhouette (amber) rotated to heading.
 *     Airline detection: callsign matches 2–3 uppercase letters followed by a digit.
 *   - On-ground aircraft: 6 px filled dot in the aircraft colour.
 *
 * Callout boxes (airborne aircraft only):
 *   - Line 1: identifier — registration if available, else ICAO24.
 *   - Line 2: aircraft type / speed knots / altitude ft.
 *   - Line 3 (airlines with route only): origin→destination.
 *   A thin connector line runs from the icon centre to the callout panel.
 *
 * Animation:
 *   Aircraft icons interpolate smoothly between poll snapshots. When new data arrives
 *   (every 10 s), prevByIcao captures the current interpolated lat/lon/heading and
 *   interpAnim drives animFraction 0→1 over 10 s. A 30 fps Timer calls
 *   rebuildAnimatedList(), which lerps each aircraft's position and feeds animatedList
 *   to the icon canvas. Callout boxes snap to new positions at each poll — acceptable
 *   for text labels.
 */
import QtQuick
import PairionScene 1.0

Item {
    id: root

    // ── OverlayBase contract ──────────────────────────────────────

    /**
     * @brief Reference to the active background instance.
     * Calls background.latLonToScreen(lat, lon) for coordinate mapping.
     */
    property var background: null

    /** @brief Parameters for this overlay from the most recent OverlayAdd command. */
    property var overlayParams: ({})

    /** @brief Model data keyed by modelId. */
    property var sceneData: ({})

    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Aircraft data ─────────────────────────────────────────────

    /**
     * @brief Live aircraft array from SceneDataPush (modelId="adsb").
     * Uses a length-check guard instead of Array.isArray(): Qt 6 delivers
     * QVariantList through QVariantMap as a list-like object that is not a
     * native JS Array, so Array.isArray() returns false even for valid data.
     */
    readonly property var aircraftList: {
        var raw = sceneData["adsb"]
        if (!raw || typeof raw.length !== "number" || raw.length === 0) return []
        // Deduplicate by icao24 — OpenSky occasionally returns duplicate state vectors.
        var seen = {}
        var out  = []
        for (var i = 0; i < raw.length; i++) {
            var ac = raw[i]
            if (ac && ac.icao24 && !seen[ac.icao24]) {
                seen[ac.icao24] = true
                out.push(ac)
            }
        }
        return out
    }

    // ── Animation state ───────────────────────────────────────────

    /**
     * @brief Previous interpolated positions keyed by icao24, captured on each
     * data update. Provides the "from" end of each animation segment.
     */
    property var prevByIcao: ({})

    /**
     * @brief Current target positions keyed by icao24, from the latest
     * aircraftList snapshot. Provides the "to" end of each animation segment.
     */
    property var curByIcao: ({})

    /**
     * @brief Interpolation fraction driven by interpAnim: 0 = prev positions,
     * 1 = current positions.
     */
    property real animFraction: 0.0

    /**
     * @brief Smoothly interpolated aircraft list rebuilt at 30 fps by
     * rebuildAnimatedList(). Drives the icon Repeater.
     */
    property var animatedList: []

    /**
     * @brief Interpolated positions keyed by icao24, rebuilt in parallel with
     * animatedList. Callout delegates bind their iconX/iconY through this map
     * so they move with the icons without the Repeater recreating delegates
     * at 30 fps.
     */
    property var animatedByIcao: ({})

    /**
     * @brief When new poll data arrives, snapshot current interpolated positions
     * as prevByIcao, build curByIcao from incoming data, and restart the
     * interpolation animation.
     */
    onAircraftListChanged: {
        var newPrev = {}
        for (var i = 0; i < animatedList.length; i++) {
            var a = animatedList[i]
            newPrev[a.icao24] = { lat: a.lat, lon: a.lon, trackDeg: a.trackDeg }
        }
        prevByIcao = newPrev

        var newCur = {}
        var list   = aircraftList
        for (var j = 0; j < list.length; j++) {
            var ac = list[j]
            newCur[ac.icao24] = ac
        }
        curByIcao = newCur

        animFraction = 0.0
        interpAnim.restart()
    }

    /**
     * @brief Rebuilds animatedList by linearly interpolating each aircraft's
     * lat, lon, and trackDeg between prevByIcao and curByIcao by animFraction.
     * Called at ~30 fps by renderTimer.
     */
    function rebuildAnimatedList() {
        var f    = animFraction
        var keys = Object.keys(curByIcao)
        var out  = []
        for (var i = 0; i < keys.length; i++) {
            var id  = keys[i]
            var cur = curByIcao[id]
            var prv = prevByIcao[id]
            if (!prv) {
                out.push(cur)
                continue
            }
            // Heading interpolation — take the shortest arc across the 0/360 wrap.
            var dh = cur.trackDeg - prv.trackDeg
            if (dh >  180) dh -= 360
            if (dh < -180) dh += 360
            out.push({
                icao24:       cur.icao24,
                callsign:     cur.callsign,
                lat:          prv.lat      + (cur.lat      - prv.lat)  * f,
                lon:          prv.lon      + (cur.lon      - prv.lon)  * f,
                altitudeFt:   cur.altitudeFt,
                speedKnots:   cur.speedKnots,
                trackDeg:     prv.trackDeg + dh                        * f,
                onGround:     cur.onGround,
                registration: cur.registration,
                aircraftType: cur.aircraftType,
                origin:       cur.origin,
                destination:  cur.destination
            })
        }
        animatedList = out

        var byId = {}
        for (var k = 0; k < out.length; k++) byId[out[k].icao24] = out[k]
        animatedByIcao = byId
    }

    /**
     * @brief Drives animFraction from 0 to 1 over the poll interval so icons
     * glide smoothly from their previous positions to their new ones.
     */
    NumberAnimation {
        id: interpAnim
        target:   root
        property: "animFraction"
        from: 0.0; to: 1.0
        duration: 10000
        running:  false
    }

    /**
     * @brief 30 fps render tick — rebuilds animatedList for smooth icon movement.
     */
    Timer {
        id: renderTimer
        interval: 33
        running:  true
        repeat:   true
        onTriggered: root.rebuildAnimatedList()
    }

    // ── Coordinate helpers ────────────────────────────────────────

    /**
     * @brief Maps a longitude to a screen X pixel via the active background.
     * @param lon Longitude in decimal degrees.
     * @return Screen X in pixels, or width/2 when no background is set.
     */
    function lonToScreenX(lon) {
        return background ? background.latLonToScreen(root.homeLat, lon).x : width * 0.5
    }

    /**
     * @brief Maps a latitude to a screen Y pixel via the active background.
     * @param lat Latitude in decimal degrees.
     * @return Screen Y in pixels, or height/2 when no background is set.
     */
    function latToScreenY(lat) {
        return background ? background.latLonToScreen(lat, root.homeLon).y : height * 0.5
    }

    // Reference coordinates used only for the single-axis helpers above.
    // The actual per-aircraft mapping calls latLonToScreen with full lat+lon.
    readonly property real homeLat: 33.814427
    readonly property real homeLon: -96.582106

    // ── ICAO airline designator → full name ──────────────────────

    /**
     * @brief Lookup table mapping 3-letter ICAO airline designator to full name.
     */
    readonly property var airlineNames: ({
        "AAL": "American Airlines",   "DAL": "Delta Air Lines",
        "UAL": "United Airlines",     "SWA": "Southwest Airlines",
        "ASA": "Alaska Airlines",     "JBU": "JetBlue Airways",
        "FFT": "Frontier Airlines",   "NKS": "Spirit Airlines",
        "SKW": "SkyWest Airlines",    "ENY": "Envoy Air",
        "ASH": "Mesa Airlines",       "RPA": "Republic Airways",
        "PDT": "Piedmont Airlines",   "PSA": "PSA Airlines",
        "AWI": "Air Wisconsin",       "EJA": "NetJets",
        "FDX": "FedEx Express",       "UPS": "UPS Airlines",
        "GTI": "Atlas Air",           "ABX": "ABX Air",
        "WJA": "WestJet",             "ACA": "Air Canada",
        "BAW": "British Airways",     "DLH": "Lufthansa",
        "AFR": "Air France",          "KLM": "KLM Royal Dutch",
        "UAE": "Emirates",            "QTR": "Qatar Airways",
        "ANA": "All Nippon Airways",  "JAL": "Japan Airlines",
        "CPA": "Cathay Pacific",      "KAL": "Korean Air",
        "MXY": "Breeze Airways",      "VIR": "Virgin Atlantic",
        "CLX": "Cargolux",            "SIA": "Singapore Airlines",
    })

    // ── Helpers ───────────────────────────────────────────────────

    /**
     * @brief Returns true when the callsign matches the ICAO airline pattern
     * (2–3 uppercase letters followed by a digit).
     * @param callsign The aircraft callsign string.
     * @return True for airline flights, false for GA or unknown.
     */
    function isAirline(callsign) {
        if (!callsign) return false
        return /^[A-Z]{2,3}\d/.test(callsign)
    }

    /**
     * @brief Returns the full airline name for a Part 121 callsign, or empty
     * string if not recognised.
     * @param callsign e.g. "AAL1706" → "American Airlines".
     */
    function airlineName(callsign) {
        if (!callsign) return ""
        var m = callsign.match(/^([A-Z]{2,3})\d/)
        if (!m) return ""
        return airlineNames[m[1]] || ""
    }

    /**
     * @brief Returns the display identifier for an aircraft.
     * Part 121 flights: shows the callsign. GA flights: shows registration or ICAO24.
     * @param ac Aircraft object.
     * @return Display identifier string.
     */
    function aircraftId(ac) {
        if (isAirline(ac.callsign)) return ac.callsign
        return (ac.registration && ac.registration !== "") ? ac.registration : ac.icao24
    }

    /**
     * @brief Returns the type/speed/alt summary line for an aircraft.
     * @param ac Aircraft object.
     * @return Formatted string, e.g. "B738  480kt  35000ft".
     */
    function aircraftSummary(ac) {
        var type  = (ac.aircraftType && ac.aircraftType !== "")
                    ? ac.aircraftType
                    : aircraftDisplayCategory(ac)
        var speed = ac.speedKnots ? Math.round(ac.speedKnots) + "kt" : "---"
        var alt   = ac.altitudeFt ? Math.round(ac.altitudeFt / 100) * 100 + "ft" : "---"
        return type + "  " + speed + "  " + alt
    }

    /**
     * @brief Returns the route string for an aircraft if both endpoints are known.
     * @param ac Aircraft object.
     * @return "KDFW→KLAX" style string, or empty string if unavailable.
     */
    function aircraftRoute(ac) {
        if (ac.origin && ac.destination && ac.origin !== "" && ac.destination !== "")
            return ac.origin + "→" + ac.destination
        return ""
    }

    /**
     * @brief Classifies an aircraft into a visual category based on its ICAO type designator.
     * @param type ICAO type code, e.g. "B738", "C172", "PC12".
     * @return Category string: "heavy" | "narrow" | "regional" | "bizjet" |
     *         "turboprop" | "piston" | "helicopter" | "unknown".
     */
    function aircraftCategory(type) {
        if (!type || type === "") return "unknown"
        var t = type.toUpperCase()
        if (/^(B74[124678X]|B748|B77[23LW]|B77F|B788|B789|B78X|A30[2678]|A330|A332|A333|A339|A342|A343|A345|A346|A350|A35K|A380|A388|DC10|L101|MD11|AN12|IL76|C5)/.test(t)) return "heavy"
        if (/^(B73[0-9]|B752|B753|B762|B763|B712|B717|A31[89]|A319|A320|A321|A20N|A21N|A318|A22N|MD8[023589]|MD90|DC9|C919|E190|E195|E290|E295)/.test(t)) return "narrow"
        if (/^(CRJ|CR[279X]|E17[05]|E75[LMS]|E14[5]|ERJ|E50[0P]|E55[0P]|RJ[17]|SF34|JS41|DH8D|AT[R]46|AT[R]72|AT76)/.test(t)) return "regional"
        if (/^(C25[0-9AB]|C56[05X]|C68[05]|C750|GL[5-7][T]|GLA|GLF[2-6]|G[2-7][0-9]|FA7[05]|FA50|LJ[2456]|PC24|CL30|CL60|GALX|HDJT|E50P|PRM1|SBR1|BE40|HA4T|P180|TBM[7-9])/.test(t)) return "bizjet"
        if (/^(C208|PC12|DH8[ABCD]|AT[457][0-9]|SF3|B190|C90|DHC[0-9]|JS32|SW4|F27|AN26|BE[34][0-9]|E120|ATR|SH36|C212)/.test(t)) return "turboprop"
        if (/^(R22|R44|R66|H60|B06|AS3[25]|EC[0-9]|AW[0-9]|UH[0-9]|BK11[07]|S76|S92|MD90|A109|H135|H145|H175)/.test(t)) return "helicopter"
        if (/^(C1[0-9][0-9]|C172|C182|C206|C210|PA[123456][0-9]|SR2[024]|DA[24][02]|M20|BE[23][0-9]|RV[0-9]|CH7[0-9]|AA5|GLOB)/.test(t)) return "piston"
        return "unknown"
    }

    /**
     * @brief Returns the display category for an aircraft, with a smarter fallback
     * when the type code is unknown. Airlines default to "narrow" (most common jet type).
     * @param ac Full aircraft object.
     * @return Category string for icon selection.
     */
    function aircraftDisplayCategory(ac) {
        var cat = aircraftCategory(ac.aircraftType)
        if (cat === "unknown") {
            if (isAirline(ac.callsign)) return "narrow"
            return "piston"
        }
        return cat
    }

    /**
     * @brief Returns the QRC path for the aircraft icon image corresponding to the category.
     * @param category One of the strings returned by aircraftCategory().
     * @return QRC resource path string.
     */
    function aircraftIconPath(category) {
        var c = (category === "helicopter") ? "piston" : category
        return "qrc:/resources/adsb/icons/" + c + ".png"
    }

    // ── Aircraft icon layer ───────────────────────────────────────

    Repeater {
        model: root.aircraftList

        /**
         * @brief Icon delegate for one aircraft.
         *
         * Screen position is driven by animatedByIcao so icons glide smoothly at 30 fps
         * without recreating delegates (model only changes every 10 s poll).
         */
        delegate: Item {
            id: iconRoot

            required property var modelData
            readonly property var  ac:       modelData
            readonly property var  animPos:  root.animatedByIcao[ac.icao24] || ac
            readonly property var  screenPt: root.background
                                             ? root.background.latLonToScreen(animPos.lat, animPos.lon)
                                             : Qt.point(-999, -999)
            readonly property real sx:       screenPt.x
            readonly property real sy:       screenPt.y
            readonly property bool inView:   sx > -40 && sx < root.width  + 40
                                          && sy > -40 && sy < root.height + 40

            // Airborne: PNG silhouette rotated to heading.
            Image {
                visible:  !ac.onGround && iconRoot.inView
                source:   root.aircraftIconPath(root.aircraftDisplayCategory(ac))
                width:    36
                height:   36
                fillMode: Image.PreserveAspectFit
                smooth:   true
                mipmap:   true
                x:        iconRoot.sx - width  / 2
                y:        iconRoot.sy - height / 2
                rotation: iconRoot.animPos.trackDeg || 0
            }

            // On-ground: small filled dot.
            Rectangle {
                visible: ac.onGround && iconRoot.inView
                width:   6; height: 6; radius: 3
                color:   root.isAirline(ac.callsign) ? PairionStyle.airlineColor : PairionStyle.gaColor
                x:       iconRoot.sx - 3
                y:       iconRoot.sy - 3
            }
        }
    }

    // ── Callout boxes ─────────────────────────────────────────────

    Repeater {
        model: root.aircraftList.filter(function(ac) { return !ac.onGround })

        /**
         * @brief Callout panel for one airborne aircraft.
         *
         * Positioned 16 px right and above the icon centre. A thin connector
         * line runs from the icon centre to the panel's bottom-left corner.
         */
        delegate: Item {
            id: calloutRoot

            required property var modelData
            readonly property var  ac:          modelData
            readonly property var  animPos:     root.animatedByIcao[ac.icao24] || ac
            readonly property var  screenPt:    root.background
                                                ? root.background.latLonToScreen(animPos.lat, animPos.lon)
                                                : Qt.point(-999, -999)
            readonly property real iconX:       screenPt.x
            readonly property real iconY:       screenPt.y
            readonly property real boxX:        iconX + 16
            readonly property real boxY:        iconY - 26 - calloutBox.height
            readonly property bool   airline:      root.isAirline(ac.callsign)
            readonly property color  acColor:      airline ? PairionStyle.airlineColor : PairionStyle.gaColor
            readonly property string route:        root.aircraftRoute(ac)
            readonly property string airlineLabel: root.airlineName(ac.callsign)

            // Connector line from icon centre to callout box bottom-left corner.
            Canvas {
                id: connector
                anchors.fill: parent

                /**
                 * @brief Draws a 1 px connector from the icon centre to the callout box.
                 */
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle  = calloutRoot.acColor
                    ctx.globalAlpha  = 0.6
                    ctx.lineWidth    = 1
                    ctx.beginPath()
                    ctx.moveTo(calloutRoot.iconX, calloutRoot.iconY)
                    ctx.lineTo(calloutRoot.boxX, calloutRoot.boxY + calloutBox.height)
                    ctx.stroke()
                }

                Component.onCompleted: requestPaint()
            }

            onIconXChanged: connector.requestPaint()
            onIconYChanged: connector.requestPaint()

            // Callout box panel.
            Rectangle {
                id: calloutBox
                x: calloutRoot.boxX
                y: calloutRoot.boxY
                width:  contentCol.implicitWidth  + 8
                height: contentCol.implicitHeight + 6
                color:        Qt.rgba(0.05, 0.09, 0.14, 0.85)
                border.color: calloutRoot.acColor
                border.width: 1
                radius:       2
                visible:      calloutRoot.iconX > -50 && calloutRoot.iconX < root.width  + 50
                           && calloutRoot.iconY > -50 && calloutRoot.iconY < root.height + 50

                Column {
                    id: contentCol
                    x: 4; y: 3
                    spacing: 1

                    // Line 1: flight number (Part 121) or registration (GA).
                    Text {
                        text:  root.aircraftId(calloutRoot.ac)
                        color: calloutRoot.acColor
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeSm
                        font.bold:      true
                    }

                    // Line 2: full airline name — Part 121 only, when known.
                    Text {
                        visible: calloutRoot.airlineLabel !== ""
                        text:    calloutRoot.airlineLabel
                        color:   calloutRoot.acColor
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeXs
                        opacity: 0.85
                    }

                    // Line 3: type / speed / altitude.
                    Text {
                        text:  root.aircraftSummary(calloutRoot.ac)
                        color: PairionStyle.dim
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeSm
                    }

                    // Line 4: route — shown for airline flights with route data only.
                    Text {
                        visible: calloutRoot.airline && calloutRoot.route !== ""
                        text:    calloutRoot.route
                        color:   PairionStyle.dim
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeXs
                    }
                }
            }
        }
    }

    // DEBUG count overlay — remove after verifying deduplication
    Text {
        z: 999; x: 10; y: height - 80
        text: "raw:" + (sceneData["adsb"] ? sceneData["adsb"].length : 0) + " deduped:" + root.aircraftList.length
        color: "#ff6600"; font.pixelSize: 10; font.family: "Courier New"
    }

    // No-data overlay — shown when aircraft list is empty.
    Text {
        anchors.centerIn: parent
        visible: root.aircraftList.length === 0
        text:    "No ADS-B data"
        color:   "#305070"
        font.family:    PairionStyle.fontFamily
        font.pixelSize: PairionStyle.fontSizeLg
    }
}
