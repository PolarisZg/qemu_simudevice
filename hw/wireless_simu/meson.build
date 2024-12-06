wireless_simu_ss = ss.source_set()
wireless_simu_ss.add(files(
  'wireless_simu.c',
  'wireless_hal.c',
  'wireless_reg.c',
  'wireless_irq.c',
  'wireless_ce.c',
  'wireless_sk_buff.c'
))

system_ss.add_all(when: 'CONFIG_WIRELESS_SIMU', if_true: wireless_simu_ss)