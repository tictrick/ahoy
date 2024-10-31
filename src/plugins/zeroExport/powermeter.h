//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(PLUGIN_ZEROEXPORT)

#ifndef __POWERMETERx_H__
#define __POWERMETERx_H__

#include <AsyncJson.h>
#include <HTTPClient.h>

#include "config/settings.h"

#if defined(ZEROEXPORT_POWERMETER_TIBBER)
#include <string.h>

#include <list>

#include "utils/dbg.h"
#include "plugins/zeroExport/lib/sml.h"
#include "utils/DynamicJsonHandler.h"

typedef struct {
    const unsigned char OBIS[6];
    void (*Fn)(double &);
    float *Arg;
} OBISHandler;
#endif

class powermeterx {
   public:
    /** powermeter
     * constructor
     */
    powermeterx() {}

    /** ~powermeter
     * destructor
     */
    ~powermeterx() {}

    /** setup
     * Initialisierung
     * @param *cfg
     * @param *mqtt
     * @param *log
     * @returns void
     */
    bool setup(IApp *app, zeroExport_t *cfg, settings_t *config, PubMqttType *mqtt, DynamicJsonHandler *log) {
        mApp = app;
        mCfg = cfg;
        mConfig = config;
        mMqtt = mqtt;
        mLog = log;

        return true;
    }

    /** loop
     * Arbeitsschleife
     * @param void
     * @returns void
     * @todo emergency
     */
    void loop(void) {
        if (millis() - mPreviousTsp <= 1000) return;  // skip when it is to fast
        mPreviousTsp = millis();

        #ifdef ZEROEXPORT_DEBUG
            if (mCfg->debug) DBGPRINTLN(F("pm Takt:"));
        #endif /*ZEROEXPORT_DEBUG*/

        bool result = false;
        float power = 0.0;

        for (u_short group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if ((!mCfg->groups[group].enabled) || (mCfg->groups[group].sleep)) continue;

            if ((millis() - mCfg->groups[group].pm_peviousTsp) < ((uint16_t)mCfg->groups[group].pm_refresh * 1000)) continue;
            mCfg->groups[group].pm_peviousTsp = millis();

            #ifdef ZEROEXPORT_DEBUG
                if (mCfg->debug) DBGPRINTLN(F("pm Do:"));
            #endif /*ZEROEXPORT_DEBUG*/

            result = false;
            power = 0.0;

            switch (mCfg->groups[group].pm_type) {
#if defined(ZEROEXPORT_POWERMETER_SHELLY)
                case zeroExportPowermeterType_t::Shelly:
                    result = getPowermeterWattsShelly(group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
                case zeroExportPowermeterType_t::Tasmota:
                    result = getPowermeterWattsTasmota(*mLog, group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_HICHI)
                case zeroExportPowermeterType_t::Hichi:
                    result = getPowermeterWattsHichi(*mLog, group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TIBBER)
                /*  Anscheinend nutzt bei mir Tibber auch diese Freq.
                    862.75 MHz - keine Verbindung
                    863.00 MHz - geht (standard) jedoch hat Tibber dann Probleme... => 4 & 5 Balken
                    863.25 MHz - geht (ohne Tibber Probleme) => 3 & 4 Balken
                */
                case zeroExportPowermeterType_t::Tibber:
                    if (mCfg->groups[group].pm_refresh < 3) mCfg->groups[group].pm_refresh = 3;
                    result = getPowermeterWattsTibber(group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_SHRDZM)
                case zeroExportPowermeterType_t::Shrdzm:
                    result = getPowermeterWattsShrdzm(group, &power);
                    break;
#endif
            }

            if (mConfig->plugin.powermeter.debug) {
                DPRINTLN(DBG_INFO, String("ze: ") + mLog->toString());
            }

            if (result) {
                bufferWrite(power, group);
                mCfg->groups[group].power = power;

                // MQTT - Powermeter
                if (mConfig->plugin.powermeter.log_over_mqtt) {
                    mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), String(ah::round1(power)).c_str(), false);
                }
            }
        }
    }

    /** getDataAVG
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataAVG(uint8_t group) {
        float avg = 0.0;

        for (int i = 0; i < 5; i++) {
            avg += mPowermeterBuffer[group][i];
        }
        avg = avg / 5.0;

        return avg;
    }

    /** getDataMIN
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataMIN(uint8_t group) {
        float min = 0.0;

        for (int i = 0; i < 5; i++) {
            if (i == 0)
                min = mPowermeterBuffer[group][i];
            if (min > mPowermeterBuffer[group][i])
                min = mPowermeterBuffer[group][i];
        }

        return min;
    }

    /** getDataMAX
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataMAX(uint8_t group) {
        float max = 0.0;

        for (int i = 0; i < 5; i++) {
            if (i == 0)
                max = mPowermeterBuffer[group][i];
            if (max < mPowermeterBuffer[group][i])
                max = mPowermeterBuffer[group][i];
        }

        return max;
    }

    /** onMqttConnect
     *
     */
    void onMqttConnect(void) {
#if defined(ZEROEXPORT_POWERMETER_MQTT)

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!strcmp(mCfg->groups[group].pm_src, "")) continue;

            if (!mCfg->groups[group].enabled) continue;

            if (mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) {
                mMqtt->subscribeExtern(String(mCfg->groups[group].pm_src).c_str(), QOS_2);
            }
        }

#endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/
    }

    /** onMqttMessage
     * This function is needed for all mqtt connections between ahoy and other devices.
     */
    bool onMqttMessage(const char* topic, const uint8_t* payload, size_t len)
    {
        bool result = false;

        #if defined(ZEROEXPORT_POWERMETER_MQTT)
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++)
            {
                if (!mCfg->groups[group].enabled) continue;
                if (!mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) continue;
                if (!strcmp(mCfg->groups[group].pm_src, "")) continue;
                if (strcmp(mCfg->groups[group].pm_src, topic) != 0) continue;    // strcmp liefert 0 wenn gleich

                float power = 0.0;
                String sPayload = String((const char*)payload).substring(0, len);

                if (sPayload.startsWith("{") && sPayload.endsWith("}") || sPayload.startsWith("[") && sPayload.endsWith("]"))
                {
                    #ifdef ZEROEXPORT_DEBUG
                        DPRINTLN(DBG_INFO, String("ze: mqtt powermeter val: ") + sPayload);
                    #endif /*ZEROEXPORT_DEBUG*/

                    DynamicJsonDocument datajson(2048); // TODO: JSON größe dynamisch machen?
                    if(!deserializeJson(datajson, sPayload.c_str()))
                    {
                        #ifdef ZEROEXPORT_DEBUG
                            DPRINTLN(DBG_INFO, String("ze: mqtt powermeter deserialize ok"));
                            DPRINTLN(DBG_INFO, String(datajson.as<String>()));
                        #endif /*ZEROEXPORT_DEBUG*/
                        power = extractJsonKey(datajson, mCfg->groups[group].pm_jsonPath);
                    }

                }
                else
                {
                    #ifdef ZEROEXPORT_DEBUG
                        DPRINTLN(DBG_INFO, String("ze: mqtt powermeter kein json"));
                    #endif /*ZEROEXPORT_DEBUG*/
                    power = sPayload.toFloat();
                }

                bufferWrite(power, group);
                mCfg->groups[group].power = power;

                // MQTT - Powermeter
                DPRINTLN(DBG_INFO, String("ze: mqtt powermeter ") + String(power));
                if (mCfg->debug) {
                    if (mMqtt->isConnected()) {
                        mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), String(ah::round1(power)).c_str(), false);
                    }
                }

                result = true;
            }

        #endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/

        return result;
    }

   private:
    /** mqttSubscribe
     * when a MQTT Msg is needed to subscribe, then a publish is leading
     * @param gr
     * @param payload
     * @returns void
     */
    void mqttSubscribe(String gr, String payload) {
        //        mqttPublish(gr, payload);
        mMqtt->subscribe(gr.c_str(), QOS_2);
    }

    /*uint16_t mqttUnsubscribe(const char *subTopic,)
    { TODO: hier weiter?
        return mMqtt->unsubscribe(topic);  // add as many topics as you like
    }*/

    /** mqttPublish
     * when a MQTT Msg is needed to Publish, but not to subscribe.
     * @param gr
     * @param payload
     * @param retain
     * @returns void
     */
    void mqttPublish(String gr, String payload, bool retain = false) {
        mMqtt->publish(gr.c_str(), payload.c_str(), retain);
    }

    HTTPClient http;

    zeroExport_t *mCfg;
    settings_t *mConfig = nullptr;
    PubMqttType *mMqtt = nullptr;
    DynamicJsonHandler *mLog;
    IApp *mApp = nullptr;

    unsigned long mPreviousTsp = millis();

    float mPowermeterBuffer[ZEROEXPORT_MAX_GROUPS][5] = {0};
    short mPowermeterBufferPos[ZEROEXPORT_MAX_GROUPS] = {0};

    StaticJsonDocument<512> mqttDoc;  // DynamicJsonDocument mqttDoc(512);
    JsonObject mqttObj = mqttDoc.to<JsonObject>();

    /** setHeader
     *
     */
    void setHeader(HTTPClient *h, String auth = "", u8_t realm = 0) {
        h->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        // TODO: Ahoy-0.8.152-2ze
        h->setUserAgent(String("Ahoy-") + mApp->getVersion());
//        h->setConnectTimeout(500);
        h->setTimeout(2000);
        h->addHeader("Content-Type", "application/json");
        h->addHeader("Accept", "application/json");

        if (auth != NULL && realm) {
            // h->addHeader("WWW-Authenticate", "Digest qop=\"auth\", realm=\"" + "shellypro4pm-f008d1d8b8b8" + "\\", nonce=\"60dc59c6\", algorithm=SHA-256");
        } else if (!auth.isEmpty()) {
            h->addHeader("Authorization", "Basic " + auth);
        }

        /*
        Shelly PM Mini Gen3
        Shelly Plus 1PM
        Shelly Plus 2PM
        Shelly Pro 3EM - 120A
        Shelly Pro 4PM
        Shelly Pro Dual Cover / Shutter PM
        Shelly Pro 1PM
        Shelly Pro 2PM
        Shelly Pro EM - 50
        Shelly Qubino Wave 1PM Mini
        Shelly Qubino Wave PM Mini
        Shelly Qubino Wave Shutter
        Shelly Qubino Wave 1PM
        Shelly Qubino Wave 2PM
        Shelly Qubino Wave Pro 1PM
        Shelly Qubino Wave Pro 2PM
        Shelly 3EM
        Shelly EM + 120A Clamp

            All Required:
            realm: string, device_id of the Shelly device.
            username: string, must be set to admin.
            nonce: number, random or pseudo-random number to prevent replay attacks, taken from the error message.
            cnonce: number, client nonce, random number generated by the client.
            response: string, encoding of the string <ha1> + ":" + <nonce> + ":" + <nc> + ":" + <cnonce> + ":" + "auth" + ":" + <ha2> in SHA256.
                ha1: string, <user>:<realm>:<password> encoded in SHA256
                ha2: string, "dummy_method:dummy_uri" encoded in SHA256
            algorithm: string, SHA-256.
        */
    }

    /**
     *
     *
     */
/*
    float extractJsonKey(DynamicJsonDocument data, const char* key)
    {
        if (data.containsKey(key))
            return (float)data[key];
        else {
            DPRINTLN(DBG_INFO, String("ze: mqtt powermeter deserialize no key ") + String(key));
            return 0.0F;
        }
    }
*/
bool findKeyInJson(JsonVariant variant, const char* key, float& value) {
    // Überprüfen, ob der aktuelle Variant ein Objekt ist
    if (variant.is<JsonObject>()) {
        JsonObject obj = variant.as<JsonObject>();

        // Durchlaufe alle Schlüssel im Objekt
        for (JsonPair pair : obj) {
            if (strcmp(pair.key().c_str(), key) == 0) {
                // Wenn der Schlüssel gefunden wird, setze den Wert
                value = (float)pair.value().as<float>();
                return true;
            }
        }

        // Durchlaufe alle Werte im Objekt und suche rekursiv
        for (JsonPair pair : obj) {
            if (findKeyInJson(pair.value(), key, value)) {
                return true;
            }
        }
    }
    // Überprüfen, ob der aktuelle Variant ein Array ist
    else if (variant.is<JsonArray>()) {
        JsonArray arr = variant.as<JsonArray>();

        // Durchlaufe das Array und suche rekursiv
        for (JsonVariant item : arr) {
            if (findKeyInJson(item, key, value)) {
                return true;
            }
        }
    }
    return false; // Schlüssel nicht gefunden
}

float extractJsonKey(DynamicJsonDocument data, const char* key) {
    float value = 0.0F;
    if (findKeyInJson(data, key, value)) {
        return value;
    } else {
        DPRINTLN(DBG_INFO, String("ze: mqtt powermeter deserialize no key ") + String(key));
        return 0.0F;
    }
}

#if defined(ZEROEXPORT_POWERMETER_SHELLY)
    /** getPowermeterWattsShelly
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsShelly(uint8_t group, float *power) {
        mLog->addProperty("mod", "getPowermeterWattsShelly");

        setHeader(&http);

        String url = String("http://") + String(mCfg->groups[group].pm_src) + String("/") + String(mCfg->groups[group].pm_jsonPath);
        mLog->addProperty("HTTP_URL", url);

        http.begin(url);

        if (http.GET() == HTTP_CODE_OK) {
            // Parsing
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, http.getString());
            if (error) {
                mLog->addProperty("err", "deserializeJson: " + String(error.c_str()));
                return false;
            } else {
                switch (mCfg->groups[group].pm_target) {
                    case zeroExportPowermeterTarget::L1:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][0]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["a_act_power"];
                        } else if (doc.containsKey(F("a_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["a_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::L2:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][1]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["b_act_power"];
                        } else if (doc.containsKey(F("b_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["b_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::L3:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][2]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["c_act_power"];
                        } else if (doc.containsKey(F("c_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["c_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::Sum:
                    default:
                        if (doc.containsKey(F("total_power"))) {
                            // Shelly 3EM
                            *power = doc["total_power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["total_act_power"];
                        } else if (doc.containsKey(F("total_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["total_act_power"];
                        }
                        break;
                }
            }
        }
        http.end();
        return true;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
    /** getPowermeterWattsTasmota
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsTasmota(DynamicJsonHandler logObj, uint8_t group, float *power) {
        logObj["mod"] = "getPowermeterWattsTasmota";
        /*
        // TODO: nicht komplett

                    HTTPClient http;
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.setUserAgent("Ahoy-Agent");
        // TODO: Ahoy-0.8.850024-zero
                    http.setConnectTimeout(500);
                    http.setTimeout(500);
        // TODO: Timeout von 1000 reduzieren?
                    http.addHeader("Content-Type", "application/json");
                    http.addHeader("Accept", "application/json");

        //            String url = String("http://") + String(mCfg->groups[group].pm_src) + String("/") + String(mCfg->groups[group].pm_jsonPath);
                    String url = String(mCfg->groups[group].pm_src);
                    logObj["HTTP_URL"] = url;

                    http.begin(url);

                    if (http.GET() == HTTP_CODE_OK)
                    {

                        // Parsing
                        DynamicJsonDocument doc(2048);
                        DeserializationError error = deserializeJson(doc, http.getString());
                        if (error)
                        {
                            logObj["error"] = "deserializeJson() failed: " + String(error.c_str());
                            return result;
                        }

        // TODO: Sum
                            result = true;

        // TODO: L1

        // TODO: L2

        // TODO: L3

        /*
                        JsonObject Tasmota_ENERGY = doc["StatusSNS"]["ENERGY"];
                        int Tasmota_Power = Tasmota_ENERGY["Power"]; // 0
                        return Tasmota_Power;
        */
        /*
        String url = "http://" + String(TASMOTA_IP) + "/cm?cmnd=status%2010";
        ParsedData = http.get(url).json();
        int Watts = ParsedData[TASMOTA_JSON_STATUS][TASMOTA_JSON_PAYLOAD_MQTT_PREFIX][TASMOTA_JSON_POWER_MQTT_LABEL].toInt();
        return Watts;
        */
        /*
                        logObj["P"]   = mCfg->groups[group].pmPower;
                        logObj["P1"] = mCfg->groups[group].pmPowerL1;
                        logObj["P2"] = mCfg->groups[group].pmPowerL2;
                        logObj["P3"] = mCfg->groups[group].pmPowerL3;
                    }
                    http.end();
        */
        return false;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_HICHI)
    /** getPowermeterWattsHichi
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsHichi(DynamicJsonHandler logObj, uint8_t group, float *power) {
        logObj["mod"] = "getPowermeterWattsHichi";

        // Hier neuer Code - Anfang

        // TODO: Noch nicht komplett

        // Hier neuer Code - Ende

        return false;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_TIBBER)
    /** getPowermeterWattsTibber
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     * @TODO: Username & Passwort wird mittels base64 verschlüsselt. Dies wird für die Authentizierung benötigt. Wichtig diese im WebUI unkenntlich zu machen und base64 im eeprom zu speichern, statt klartext.
     * @TODO: Abfrage Interval einbauen. Info: Datei-Size kann auch mal 0-bytes sein!
     */

    sml_states_t currentState;

    float _powerMeterTotal = 0.0;

    float _powerMeter1Power = 0.0;
    float _powerMeter2Power = 0.0;
    float _powerMeter3Power = 0.0;

    float _powerMeterImport = 0.0;
    float _powerMeterExport = 0.0;

    /*
     07 81 81 c7 82 03 ff		#objName: OBIS Kennzahl für den Hersteller
     07 01 00 01 08 00 ff		#objName: OBIS Kennzahl für Wirkenergie Bezug gesamt tariflos
     07 01 00 01 08 01 ff 		#objName: OBIS-Kennzahl für Wirkenergie Bezug Tarif1
     07 01 00 01 08 02 ff		#objName: OBIS-Kennzahl für Wirkenergie Bezug Tarif2
     07 01 00 02 08 00 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung gesamt tariflos
     07 01 00 02 08 01 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung Tarif1
     07 01 00 02 08 02 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung Tarif2
    */
    const std::list<OBISHandler> smlHandlerList{
        {{0x01, 0x00, 0x10, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeterTotal},   // total - OBIS-Kennzahl für momentane Gesamtwirkleistung
        {{0x01, 0x00, 0x24, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter1Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L1
        {{0x01, 0x00, 0x38, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter2Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L2
        {{0x01, 0x00, 0x4c, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter3Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L3
        {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterImport},
        {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterExport}};

    /*
    Daniel92: https://tibber.com/de/api/lookup/price-overview?postalCode=
    Hab ich mal ausgelesen... hintendran die PLZ eingeben
    energy/todayHours/<aktuelleStunde>/priceIncludingVat
    */
    bool getPowermeterWattsTibber(uint8_t group, float *power) {
        bool result = false;
        mLog->addProperty("mod", "getPowermeterWattsTibber");

        String url = String("http://");
//        url +=  String(mCfg->groups[group].pm_user) + ":" + String(mCfg->groups[group].pm_pass) + "@";
        url +=  String(mCfg->groups[group].pm_src) +  "/" + String(mCfg->groups[group].pm_jsonPath);

        http.begin(url);
        setHeader(&http, String(mCfg->groups[group].pm_cred));

        if (mCfg->debug) {
            mLog->addProperty("url", url);
            mLog->addProperty("cred", String(mCfg->groups[group].pm_cred));
        }

        int get = http.GET();
        int size = http.getSize();
        String payload = http.getString();

        if (mCfg->debug) {
            mLog->addProperty("http.Get", String(get));
            mLog->addProperty("http.getSize", String(size));
            mLog->addProperty("http.getString", payload);
        }

        if (get == HTTP_CODE_OK && size > 0) {
            double readVal = 0;
            unsigned char c;

            for (int i = 0; i < size; ++i) {
                c = payload[i];
                sml_states_t smlCurrentState = smlState(c);

                switch (smlCurrentState) {
                    case SML_FINAL:
                        *power = _powerMeterTotal;
// TODO: pm_taget auswerten und damit eine Regelung auf Sum, L1, L2, L3 ermöglichen (setup.html nicht vergessen)
                        if (mCfg->debug) mLog->addProperty("power", String(*power));
                        result = true;
                        break;
                    case SML_LISTEND:
                        // check handlers on last received list
                        for (auto &handler : smlHandlerList) {
                            if (smlOBISCheck(handler.OBIS)) {
                                handler.Fn(readVal);
                                *handler.Arg = readVal;
                            }
                        }
                        break;
                }
            }
        }

        http.end();
        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_SHRDZM)
    /** getPowermeterWattsShrdzm
     * Danke an Obmar für die Bereitstellung eines SHRDZM über Internet.
     * @param logObj
     * @param group
     * @returns true/false
     * @TODO: Username & Passwort wird mittels base64 verschlüsselt. Dies wird für die Authentizierung benötigt. Wichtig diese im WebUI unkenntlich zu machen und base64 im eeprom zu speichern, statt klartext.
     * @TODO: Abfrage Interval einbauen. Info: Datei-Size kann auch mal 0-bytes sein?
     */
    bool getPowermeterWattsShrdzm(uint8_t group, float *power) {
        mLog->addProperty("mod", "getPowermeterWattsShrdzm");

        String url = String("http://") + String(mCfg->groups[group].pm_src);

        http.begin(url);
        setHeader(&http, String(mCfg->groups[group].pm_cred));

        if (mCfg->debug) {
            mLog->addProperty("url", url);
            mLog->addProperty("cred", String(mCfg->groups[group].pm_cred));
        }

        int get = http.GET();
        int size = http.getSize();
        String payload = http.getString();

        if (mCfg->debug) {
            mLog->addProperty("http.Get", String(get));
            mLog->addProperty("http.getSize", String(size));
        }

        if (get == HTTP_CODE_OK && size > 0) {
            // Parsing
            DynamicJsonDocument doc(size + 256);
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                if (mCfg->debug) {
                    mLog->addProperty("http.getString", "payload: " + payload);
                }
                mLog->addProperty("err", "deserializeJson: " + String(error.c_str()));
                return false;
            } else {
                *power = extractJsonKey(doc, mCfg->groups[group].pm_jsonPath);
                if (mCfg->debug) mLog->addProperty("power", String(*power));
            }
        }
        http.end();

        return true;
    }
#endif

    /**
     *
     */
    void bufferWrite(float raw, short group) {
        mPowermeterBuffer[group][mPowermeterBufferPos[group]] = raw;
        mPowermeterBufferPos[group] = (mPowermeterBufferPos[group] + 1) % 5;
    }
};

#endif /*__POWERMETERx_H__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */
