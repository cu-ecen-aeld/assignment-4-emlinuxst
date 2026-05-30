################################################################################
#
# AESD-ASSIGNMENTS
#
################################################################################

AESD_ASSIGNMENTS_VERSION = 180956e6a7f7348a51dba868b2ed40b5389cefe2
AESD_ASSIGNMENTS_SITE = https://github.com/cu-ecen-aeld/assignment-4-emlinuxst.git
AESD_ASSIGNMENTS_SITE_METHOD = git
AESD_ASSIGNMENTS_GIT_SUBMODULES = YES

define AESD_ASSIGNMENTS_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)/server all
endef

define AESD_ASSIGNMENTS_INSTALL_TARGET_CMDS
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/bin
	$(INSTALL) -m 0755 $(@D)/server/aesdsocket $(TARGET_DIR)/usr/bin/aesdsocket
endef

$(eval $(generic-package))
