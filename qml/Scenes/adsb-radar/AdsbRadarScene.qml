/**
 * @file AdsbRadarScene.qml
 * @brief ADS-B radar scene — live aircraft overlay on an embedded FAA VFR sectional chart.
 *
 * Displays aircraft positions received via SceneDataPush (modelId="adsb") on top of
 * a pre-processed FAA VFR Sectional chart image embedded as a Qt resource. The image
 * (resources/adsb/vfr_sectional.png) covers exactly the ADS-B bounding box:
 *   lamin=33.398  lomin=-97.084  lamax=34.231  lomax=-96.080
 *
 * Coordinate mapping is a simple linear projection — no Mercator math required because
 * the image already spans the exact same bounding box as the aircraft data:
 *   screenX = (lon - lomin) / (lomax - lomin) * width
 *   screenY = (lamax - lat) / (lamax - lamin) * height
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
 * Range rings are drawn at 5 nm, 10 nm, and 25 nm from the home location at low
 * opacity (0.25) so the sectional chart remains the dominant visual layer.
 *
 * Animation:
 *   Aircraft icons interpolate smoothly between poll snapshots. When new data arrives
 *   (every 10 s), prevByIcao captures the current interpolated lat/lon/heading and
 *   interpAnim drives animFraction 0→1 over 10 s. A 30 fps Timer calls
 *   rebuildAnimatedList(), which lerps each aircraft's position and feeds animatedList
 *   to the icon canvas. Callout boxes snap to new positions at each poll — acceptable
 *   for text labels.
 *
 * To regenerate the sectional PNG after an FAA chart cycle (every 56 days), run:
 *   python3 scripts/process_vfr_sectional.py
 */
import QtQuick
import Pairion
import PairionScene 1.0

Item {
    id: root

    // ── SceneBase contract ────────────────────────────────────────

    /** Server-pushed model data keyed by modelId. */
    property var    sceneData:   ({})
    /** Parameters from the activating SceneChange command. */
    property var    sceneParams: ({})
    /** Current agent state string from SceneManager. */
    property string hudState:    "idle"

    /** @brief Request scene switch (forwarded to SceneManager when needed). */
    signal requestScene(string sceneId, var params)
    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Bounding box constants (must match application.yml pairion.data.adsb) ──

    /** @brief Southern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamin:  33.398
    /** @brief Western boundary of the ADS-B coverage area (degrees). */
    readonly property real lomin: -97.084
    /** @brief Northern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamax:  34.231
    /** @brief Eastern boundary of the ADS-B coverage area (degrees). */
    readonly property real lomax: -96.080

    /** @brief Home location latitude — centre of range rings. */
    readonly property real homeLat: 33.814427
    /** @brief Home location longitude — centre of range rings. */
    readonly property real homeLon: -96.582106

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
     * rebuildAnimatedList(). Drives the icon canvas.
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
        // Snapshot the current interpolated positions as the new "from".
        var newPrev = {}
        for (var i = 0; i < animatedList.length; i++) {
            var a = animatedList[i]
            newPrev[a.icao24] = { lat: a.lat, lon: a.lon, trackDeg: a.trackDeg }
        }
        prevByIcao = newPrev

        // Build the new "to" map from incoming data.
        var newCur = {}
        var list   = aircraftList
        for (var j = 0; j < list.length; j++) {
            var ac = list[j]
            newCur[ac.icao24] = ac
        }
        curByIcao = newCur

        // Reset and restart the interpolation.
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
                // New aircraft — no previous position, appear immediately.
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

        // Build parallel map for callout delegate position bindings.
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
     * @brief 30 fps render tick — rebuilds animatedList for the icon canvas.
     * Runs continuously so icons move smoothly between poll snapshots.
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
     * @brief Maps a longitude to a screen X pixel using linear interpolation
     * across the bounding box.
     * @param lon Longitude in decimal degrees.
     * @return Screen X in pixels.
     */
    function lonToScreenX(lon) {
        return (lon - lomin) / (lomax - lomin) * width
    }

    /**
     * @brief Maps a latitude to a screen Y pixel using linear interpolation
     * across the bounding box. Latitude is inverted: higher lat → smaller Y.
     * @param lat Latitude in decimal degrees.
     * @return Screen Y in pixels.
     */
    function latToScreenY(lat) {
        return (lamax - lat) / (lamax - lamin) * height
    }

    // ── ICAO airline designator → full name ──────────────────────

    /**
     * @brief Lookup table mapping 3-letter ICAO airline designator to full name.
     * Used to surface airline identity for Part 121 flights in callout boxes.
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
     * Part 121 flights: shows the callsign (flight number is the meaningful ID).
     * GA flights: shows registration, falling back to ICAO24.
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
     * @brief Classifies an aircraft into a visual category based on its ICAO
     * type designator. Used to select the correct icon silhouette.
     * @param type ICAO type code, e.g. "B738", "C172", "PC12".
     * @return Category string: "heavy" | "narrow" | "regional" | "bizjet" |
     *         "turboprop" | "piston" | "helicopter" | "unknown".
     */
    function aircraftCategory(type) {
        if (!type || type === "") return "unknown"
        var t = type.toUpperCase()
        // Wide-body / heavy jets
        if (/^(B74[124678X]|B748|B77[23LW]|B77F|B788|B789|B78X|A30[2678]|A330|A332|A333|A339|A342|A343|A345|A346|A350|A35K|A380|A388|DC10|L101|MD11|AN12|IL76|C5)/.test(t)) return "heavy"
        // Narrow-body jets
        if (/^(B73[0-9]|B752|B753|B762|B763|B712|B717|A31[89]|A319|A320|A321|A20N|A21N|A318|A22N|MD8[023589]|MD90|DC9|C919|E190|E195|E290|E295)/.test(t)) return "narrow"
        // Regional jets
        if (/^(CRJ|CR[279X]|E17[05]|E75[LMS]|E14[5]|ERJ|E50[0P]|E55[0P]|RJ[17]|SF34|JS41|DH8D|AT[R]46|AT[R]72|AT76)/.test(t)) return "regional"
        // Business / light jets
        if (/^(C25[0-9AB]|C56[05X]|C68[05]|C750|GL[5-7][T]|GLA|GLF[2-6]|G[2-7][0-9]|FA7[05]|FA50|LJ[2456]|PC24|CL30|CL60|GALX|HDJT|E50P|PRM1|SBR1|BE40|HA4T|P180|TBM[7-9])/.test(t)) return "bizjet"
        // Turboprops (commercial/utility)
        if (/^(C208|PC12|DH8[ABCD]|AT[457][0-9]|SF3|B190|C90|DHC[0-9]|JS32|SW4|F27|AN26|BE[34][0-9]|E120|ATR|SH36|C212)/.test(t)) return "turboprop"
        // Helicopters
        if (/^(R22|R44|R66|H60|B06|AS3[25]|EC[0-9]|AW[0-9]|UH[0-9]|BK11[07]|S76|S92|MD90|A109|H135|H145|H175)/.test(t)) return "helicopter"
        // GA piston
        if (/^(C1[0-9][0-9]|C172|C182|C206|C210|PA[123456][0-9]|SR2[024]|DA[24][02]|M20|BE[23][0-9]|RV[0-9]|CH7[0-9]|AA5|GLOB)/.test(t)) return "piston"
        return "unknown"
    }

    /**
     * @brief Returns the QRC path for the aircraft icon image corresponding to
     * the given category. Helicopters fall back to the piston icon since no
     * dedicated helicopter silhouette is bundled.
     * @param category One of the strings returned by aircraftCategory().
     * @return QRC resource path string.
     */
    /**
     * @brief Returns the display category for an aircraft, with a smarter fallback
     * when the type code is unknown. Airlines default to "narrow" (most common jet
     * type) rather than "unknown" (military silhouette).
     * @param ac Full aircraft object.
     * @return Category string for icon and classification.
     */
    function aircraftDisplayCategory(ac) {
        var cat = aircraftCategory(ac.aircraftType)
        if (cat === "unknown") {
            // Airlines with no type code are almost certainly narrow/regional jets.
            if (isAirline(ac.callsign)) return "narrow"
            // All other unidentified aircraft: use piston silhouette as neutral fallback.
            // Never show the "unknown" (military) icon for civil traffic.
            return "piston"
        }
        return cat
    }

    function aircraftIconPath(category) {
        var c = (category === "helicopter") ? "piston" : category
        return "qrc:/resources/adsb/icons/" + c + ".png"
    }

    // ── Layers ────────────────────────────────────────────────────

    // Dark base — shown during image load and behind transparent PNG edges.
    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }

    // FAA VFR Sectional chart — hidden during ADS-B debugging; re-enable by removing visible:false.
    Image {
        id: chartImage
        anchors.fill: parent
        source:   "qrc:/resources/adsb/vfr_sectional.png"
        fillMode: Image.Stretch
        smooth:   true
        visible:  false
        opacity: status === Image.Ready ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 400 } }
    }

    // Range rings — drawn at 5 nm, 10 nm, 25 nm from home at low opacity.
    Canvas {
        id: rings
        anchors.fill: parent
        opacity: 0.25

        /**
         * @brief Draws concentric range rings from the home location.
         *
         * Pixel radius is derived from the latitude scale:
         *   pxPerNm = height / (lamax - lamin) / 60
         * This gives slightly elliptical circles because longitude pixels differ,
         * which is acceptable at this scale for range reference purposes.
         */
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Pixels per nautical mile in the Y direction.
            var pxPerNm = height / (root.lamax - root.lamin) / 60.0

            var cx = root.lonToScreenX(root.homeLon)
            var cy = root.latToScreenY(root.homeLat)

            ctx.strokeStyle = "#00b4ff"
            ctx.lineWidth   = 1
            ctx.setLineDash([4, 6])
            ctx.font        = "9px 'Courier New'"
            ctx.fillStyle   = "#00b4ff"
            ctx.textAlign   = "left"

            var ringSets = [
                { nm: 5,  label: "5nm"  },
                { nm: 10, label: "10nm" },
                { nm: 25, label: "25nm" }
            ]

            for (var i = 0; i < ringSets.length; i++) {
                var r = ringSets[i].nm * pxPerNm
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                ctx.stroke()
                ctx.fillText(ringSets[i].label, cx + 4, cy - r + 12)
            }

            // Centre crosshair at home location.
            ctx.strokeStyle = "#00b4ff"
            ctx.lineWidth   = 1
            ctx.setLineDash([2, 4])
            ctx.beginPath()
            ctx.moveTo(cx - 10, cy); ctx.lineTo(cx + 10, cy)
            ctx.moveTo(cx, cy - 10); ctx.lineTo(cx, cy + 10)
            ctx.stroke()
            ctx.setLineDash([])
        }

        Component.onCompleted: requestPaint()
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()
    }

    // Aircraft icon layer — one Image (or dot) per aircraft, animated via animatedByIcao.
    Repeater {
        model: root.aircraftList

        /**
         * @brief Icon delegate for one aircraft.
         *
         * Position is driven by animatedByIcao so icons glide smoothly at 30 fps
         * without recreating delegates (model only changes every 10 s poll).
         * Airborne aircraft use the category PNG icon rotated to heading.
         * On-ground aircraft are rendered as a 6 px filled dot.
         */
        delegate: Item {
            id: iconRoot

            required property var modelData
            readonly property var  ac:      modelData
            readonly property var  animPos: root.animatedByIcao[ac.icao24] || ac
            readonly property real sx:      root.lonToScreenX(animPos.lon)
            readonly property real sy:      root.latToScreenY(animPos.lat)
            readonly property bool inView:  sx > -40 && sx < root.width  + 40
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

    // Callout boxes — one Item per airborne aircraft, bound to aircraftList (10 s snap).
    Repeater {
        model: root.aircraftList.filter(function(ac) { return !ac.onGround })

        /**
         * @brief Callout panel for one airborne aircraft.
         *
         * Positioned 16 px right and above the icon centre. A thin connector
         * line runs from the icon centre to the panel's bottom-left corner.
         * Callouts snap to new positions at each poll (every 10 s); the icon
         * canvas animates smoothly between polls.
         */
        delegate: Item {
            id: calloutRoot

            required property var modelData
            readonly property var  ac:       modelData

            // animPos resolves the interpolated position from animatedByIcao.
            // Falls back to ac itself so the box renders immediately on first poll.
            readonly property var  animPos:  root.animatedByIcao[ac.icao24] || ac

            readonly property real iconX:    root.lonToScreenX(animPos.lon)
            readonly property real iconY:    root.latToScreenY(animPos.lat)
            readonly property real boxX:     iconX + 16
            readonly property real boxY:     iconY - 26 - calloutBox.height
            readonly property bool   airline:     root.isAirline(ac.callsign)
            readonly property color  acColor:     airline ? PairionStyle.airlineColor : PairionStyle.gaColor
            readonly property string route:       root.aircraftRoute(ac)
            readonly property string airlineLabel: root.airlineName(ac.callsign)

            // Connector line from icon centre to callout box bottom-left corner.
            Canvas {
                id: connector
                anchors.fill: parent

                /**
                 * @brief Draws a 1 px connector from the icon centre to the callout box.
                 * Repaints whenever the icon position changes (30 fps while animating).
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
