//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(PLUGIN_POWERMETER)

#ifndef __POWERMETER_H__
#define __POWERMETER_H__





template <class HMSYSTEM>

class Powermeter {
   public:
    /** Powermeter
     * constructor
     */
    Powermeter() {}

    /** ~Powermeter
     * destructor
     */
    ~Powermeter() {}

    /** setup
     * Initialisierung
     * @param *cfg
     * @param *sys
     * @param *config
     * @param *api
     * @param *mqtt
     * @returns void
     */
    void setup(IApp *app, uint32_t *timestamp, powermeter_t *cfg, HMSYSTEM *sys, settings_t *config, RestApiType *api, PubMqttType *mqtt) {
    }

    /** loop
     * Arbeitsschleife
     * @param void
     * @returns void
     * @todo emergency
     */
    void loop(void) {
    }

   private:
};





#endif /*__POWERMETER_H__*/

#endif /* #if defined(PLUGIN_POWERMETER) */
