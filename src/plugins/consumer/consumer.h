//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(PLUGIN_CONSUMER)

#ifndef __CONSUMER_H__
#define __CONSUMER_H__





template <class HMSYSTEM>

class Consumer {
   public:
    /** Consumer
     * constructor
     */
    Consumer() {}

    /** ~Consumer
     * destructor
     */
    ~Consumer() {}

    /** setup
     * Initialisierung
     * @param *cfg
     * @param *sys
     * @param *config
     * @param *api
     * @param *mqtt
     * @returns void
     */
    void setup(IApp *app, uint32_t *timestamp, consumer_t *cfg, HMSYSTEM *sys, settings_t *config, RestApiType *api, PubMqttType *mqtt) {
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





#endif /*__CONSUMER_H__*/

#endif /* #if defined(PLUGIN_CONSUMER) */
