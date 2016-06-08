// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/tpm_metrics.h"

#include <trousers/trousers.h>

namespace cryptohome {

#define TSS_ERROR_LAYER(x)  (x & 0x3000)
#define TSS_ERROR_CODE(x) (x & TSS_MAX_ERROR)

TpmResult GetTpmResultSample(TSS_RESULT result) {
  // Error Codes common to all layers.
  switch (TSS_ERROR_CODE(result)) {
    case TSS_SUCCESS:
      return kTpmSuccess;
    default:
      break;
  }

  // The return code is either unknown, or specific to a layer.
  if (TSS_ERROR_LAYER(result) == TSS_LAYER_TPM) {
    switch (TSS_ERROR_CODE(result)) {
      case TPM_E_AUTHFAIL:
        return kTpmErrorAuthenticationFail;
      case TPM_E_BAD_PARAMETER:
        return kTpmErrorBadParameter;
      case TPM_E_BADINDEX:
        return kTpmErrorBadIndex;
      case TPM_E_AUDITFAILURE:
        return kTpmErrorAuditFail;
      case TPM_E_CLEAR_DISABLED:
        return kTpmErrorClearDisabled;
      case TPM_E_DEACTIVATED:
        return kTpmErrorTpmDeactivated;
      case TPM_E_DISABLED:
        return kTpmErrorTpmDisabled;
      case TPM_E_FAIL:
        return kTpmErrorFailed;
      case TPM_E_BAD_ORDINAL:
        return kTpmErrorBadOrdinal;
      case TPM_E_INSTALL_DISABLED:
        return kTpmErrorOwnerInstallDisabled;
      case TPM_E_INVALID_KEYHANDLE:
        return kTpmErrorInvalidKeyHandle;
      case TPM_E_KEYNOTFOUND:
        return kTpmErrorKeyNotFound;
      case TPM_E_INAPPROPRIATE_ENC:
        return kTpmErrorBadEncryptionScheme;
      case TPM_E_MIGRATEFAIL:
        return kTpmErrorMigrationAuthorizationFail;
      case TPM_E_INVALID_PCR_INFO:
        return kTpmErrorInvalidPcrInfo;
      case TPM_E_NOSPACE:
        return kTpmErrorNoSpaceToLoadKey;
      case TPM_E_NOSRK:
        return kTpmErrorNoSrk;
      case TPM_E_NOTSEALED_BLOB:
        return kTpmErrorInvalidEncryptedBlob;
      case TPM_E_OWNER_SET:
        return kTpmErrorOwnerAlreadySet;
      case TPM_E_RESOURCES:
        return kTpmErrorNotEnoughTpmResources;
      case TPM_E_SHORTRANDOM:
        return kTpmErrorRandomStringTooShort;
      case TPM_E_SIZE:
        return kTpmErrorTpmOutOfSpace;
      case TPM_E_WRONGPCRVAL:
        return kTpmErrorWrongPcrValue;
      case TPM_E_BAD_PARAM_SIZE:
        return kTpmErrorBadParamSize;
      case TPM_E_SHA_THREAD:
        return kTpmErrorNoSha1Thread;
      case TPM_E_SHA_ERROR:
        return kTpmErrorSha1Error;
      case TPM_E_FAILEDSELFTEST:
        return kTpmErrorTpmSelfTestFailed;
      case TPM_E_AUTH2FAIL:
        return kTpmErrorSecondAuthorizationFailed;
      case TPM_E_BADTAG:
        return kTpmErrorBadTag;
      case TPM_E_IOERROR:
        return kTpmErrorIOError;
      case TPM_E_ENCRYPT_ERROR:
        return kTpmErrorEncryptionError;
      case TPM_E_DECRYPT_ERROR:
        return kTpmErrorDecryptionError;
      case TPM_E_INVALID_AUTHHANDLE:
        return kTpmErrorInvalidAuthorizationHandle;
      case TPM_E_NO_ENDORSEMENT:
        return kTpmErrorNoEndorsement;
      case TPM_E_INVALID_KEYUSAGE:
        return kTpmErrorInvalidKeyUsage;
      case TPM_E_WRONG_ENTITYTYPE:
        return kTpmErrorWrongEntityType;
      case TPM_E_INVALID_POSTINIT:
        return kTpmErrorInvalidPostInitSequence;
      case TPM_E_INAPPROPRIATE_SIG:
        return kTpmErrorInvalidSignatureFormat;
      case TPM_E_BAD_KEY_PROPERTY:
        return kTpmErrorBadKeyProperty;
      case TPM_E_BAD_MIGRATION:
        return kTpmErrorBadMigration;
      case TPM_E_BAD_SCHEME:
        return kTpmErrorBadScheme;
      case TPM_E_BAD_DATASIZE:
        return kTpmErrorBadDataSize;
      case TPM_E_BAD_MODE:
        return kTpmErrorBadModeParameter;
      case TPM_E_BAD_PRESENCE:
        return kTpmErrorBadPresenceValue;
      case TPM_E_BAD_VERSION:
        return kTpmErrorBadVersion;
      case TPM_E_NO_WRAP_TRANSPORT:
        return kTpmErrorWrapTransportNotAllowed;
      case TPM_E_AUDITFAIL_UNSUCCESSFUL:
        return kTpmErrorAuditFailCommandUnsuccessful;
      case TPM_E_AUDITFAIL_SUCCESSFUL:
        return kTpmErrorAuditFailCommandSuccessful;
      case TPM_E_NOTRESETABLE:
        return kTpmErrorPcrRegisterNotResetable;
      case TPM_E_NOTLOCAL:
        return kTpmErrorPcrRegisterResetRequiresLocality;
      case TPM_E_BAD_TYPE:
        return kTpmErrorBadTypeOfIdentityBlob;
      case TPM_E_INVALID_RESOURCE:
        return kTpmErrorBadResourceType;
      case TPM_E_NOTFIPS:
        return kTpmErrorCommandAvailableOnlyInFipsMode;
      case TPM_E_INVALID_FAMILY:
        return kTpmErrorInvalidFamilyId;
      case TPM_E_NO_NV_PERMISSION:
        return kTpmErrorNoNvRamPermission;
      case TPM_E_REQUIRES_SIGN:
        return kTpmErrorSignedCommandRequired;
      case TPM_E_KEY_NOTSUPPORTED:
        return kTpmErrorNvRamKeyNotSupported;
      case TPM_E_AUTH_CONFLICT:
        return kTpmErrorAuthorizationConflict;
      case TPM_E_AREA_LOCKED:
        return kTpmErrorNvRamAreaLocked;
      case TPM_E_BAD_LOCALITY:
        return kTpmErrorBadLocality;
      case TPM_E_READ_ONLY:
        return kTpmErrorNvRamAreaReadOnly;
      case TPM_E_PER_NOWRITE:
        return kTpmErrorNvRamAreaNoWriteProtection;
      case TPM_E_FAMILYCOUNT:
        return kTpmErrorFamilyCountMismatch;
      case TPM_E_WRITE_LOCKED:
        return kTpmErrorNvRamAreaWriteLocked;
      case TPM_E_BAD_ATTRIBUTES:
        return kTpmErrorNvRamAreaBadAttributes;
      case TPM_E_INVALID_STRUCTURE:
        return kTpmErrorInvalidStructure;
      case TPM_E_KEY_OWNER_CONTROL:
        return kTpmErrorKeyUnderOwnerControl;
      case TPM_E_BAD_COUNTER:
        return kTpmErrorBadCounterHandle;
      case TPM_E_NOT_FULLWRITE:
        return kTpmErrorNotAFullWrite;
      case TPM_E_CONTEXT_GAP:
        return kTpmErrorContextGap;
      case TPM_E_MAXNVWRITES:
        return kTpmErrorMaxNvRamWrites;
      case TPM_E_NOOPERATOR:
        return kTpmErrorNoOperator;
      case TPM_E_RESOURCEMISSING:
        return kTpmErrorResourceMissing;
      case TPM_E_DELEGATE_LOCK:
        return kTpmErrorDelagteLocked;
      case TPM_E_DELEGATE_FAMILY:
        return kTpmErrorDelegateFamily;
      case TPM_E_DELEGATE_ADMIN:
        return kTpmErrorDelegateAdmin;
      case TPM_E_TRANSPORT_NOTEXCLUSIVE:
        return kTpmErrorTransportNotExclusive;
      case TPM_E_OWNER_CONTROL:
        return kTpmErrorOwnerControl;
      case TPM_E_DAA_RESOURCES:
        return kTpmErrorDaaResourcesNotAvailable;
      case TPM_E_DAA_INPUT_DATA0:
        return kTpmErrorDaaInputData0;
      case TPM_E_DAA_INPUT_DATA1:
        return kTpmErrorDaaInputData1;
      case TPM_E_DAA_ISSUER_SETTINGS:
        return kTpmErrorDaaIssuerSettings;
      case TPM_E_DAA_TPM_SETTINGS:
        return kTpmErrorDaaTpmSettings;
      case TPM_E_DAA_STAGE:
        return kTpmErrorDaaStage;
      case TPM_E_DAA_ISSUER_VALIDITY:
        return kTpmErrorDaaIssuerValidity;
      case TPM_E_DAA_WRONG_W:
        return kTpmErrorDaaWrongW;
      case TPM_E_BAD_HANDLE:
        return kTpmErrorBadHandle;
      case TPM_E_BAD_DELEGATE:
        return kTpmErrorBadDelegate;
      case TPM_E_BADCONTEXT:
        return kTpmErrorBadContextBlob;
      case TPM_E_TOOMANYCONTEXTS:
        return kTpmErrorTooManyContexts;
      case TPM_E_MA_TICKET_SIGNATURE:
        return kTpmErrorMigrationAuthoritySignatureFail;
      case TPM_E_MA_DESTINATION:
        return kTpmErrorMigrationDestinationNotAuthenticated;
      case TPM_E_MA_SOURCE:
        return kTpmErrorBadMigrationSource;
      case TPM_E_MA_AUTHORITY:
        return kTpmErrorBadMigrationAuthority;
      case TPM_E_PERMANENTEK:
        return kTpmErrorPermanentEk;
      case TPM_E_BAD_SIGNATURE:
        return kTpmErrorCmkTicketBadSignature;
      case TPM_E_NOCONTEXTSPACE:
        return kTpmErrorNoContextSpace;
      case TPM_E_RETRY:
        return kTpmErrorTpmBusyRetryLater;
      case TPM_E_NEEDS_SELFTEST:
        return kTpmErrorNeedsSelfTest;
      case TPM_E_DOING_SELFTEST:
        return kTpmErrorDoingSelfTest;
      case TPM_E_DEFEND_LOCK_RUNNING:
        return kTpmErrorDefendLockRunning;
      case TPM_E_DISABLED_CMD:
        return kTpmErrorTpmCommandDisabled;
      default:
        return kTpmErrorUnknownError;
    }
  } else if (TSS_ERROR_LAYER(result) == TSS_LAYER_TDDL) {
    switch (TSS_ERROR_CODE(result)) {
      case TSS_E_FAIL:
        return kTddlErrorGeneralFail;
      case TSS_E_BAD_PARAMETER:
        return kTddlErrorBadParameter;
      case TSS_E_INTERNAL_ERROR:
        return kTddlErrorInternalSoftwareError;
      case TSS_E_NOTIMPL:
        return kTddlErrorNotImplemented;
      case TSS_E_PS_KEY_NOTFOUND:
        return kTddlErrorKeyNotFoundInPersistentStorage;
      case TSS_E_KEY_ALREADY_REGISTERED:
        return kTddlErrorKeyAlreadyRegistered;
      case TSS_E_CANCELED:
        return kTddlErrorActionCanceledByRequest;
      case TSS_E_TIMEOUT:
        return kTddlErrorTimeout;
      case TSS_E_OUTOFMEMORY:
        return kTddlErrorOutOfMemory;
      case TSS_E_TPM_UNEXPECTED:
        return kTddlErrorUnexpectedTpmOutput;
      case TSS_E_COMM_FAILURE:
        return kTddlErrorCommunicationFailure;
      case TSS_E_TPM_UNSUPPORTED_FEATURE:
        return kTddlErrorTpmUnsupportedFeature;
      case TDDL_E_COMPONENT_NOT_FOUND:
        return kTddlErrorConnectionToTpmDeviceFailed;
      case TDDL_E_ALREADY_OPENED:
        return kTddlErrorDeviceAlreadyOpened;
      case TDDL_E_BADTAG:
        return kTddlErrorBadTag;
      case TDDL_E_INSUFFICIENT_BUFFER:
        return kTddlErrorReceiveBufferTooSmall;
      case TDDL_E_COMMAND_COMPLETED:
        return kTddlErrorCommandAlreadyCompleted;
      case TDDL_E_COMMAND_ABORTED:
        return kTddlErrorCommandAborted;
      case TDDL_E_ALREADY_CLOSED:
        return kTddlErrorDeviceDriverAlreadyClosed;
      case TDDL_E_IOERROR:
        return kTddlErrorIOError;
      default:
        return kTddlErrorUnknownError;
    }
  } else if (TSS_ERROR_LAYER(result) == TSS_LAYER_TCS) {
    switch (TSS_ERROR_CODE(result)) {
      case TSS_E_FAIL:
        return kTcsErrorGeneralFail;
      case TSS_E_BAD_PARAMETER:
        return kTcsErrorBadParameter;
      case TSS_E_INTERNAL_ERROR:
        return kTcsErrorInternalSoftwareError;
      case TSS_E_NOTIMPL:
        return kTcsErrorNotImplemented;
      case TSS_E_PS_KEY_NOTFOUND:
        return kTcsErrorKeyNotFoundInPersistentStorage;
      case TSS_E_KEY_ALREADY_REGISTERED:
        return kTcsErrorKeyAlreadyRegistered;
      case TSS_E_CANCELED:
        return kTcsErrorActionCanceledByRequest;
      case TSS_E_TIMEOUT:
        return kTcsErrorTimeout;
      case TSS_E_OUTOFMEMORY:
        return kTcsErrorOutOfMemory;
      case TSS_E_TPM_UNEXPECTED:
        return kTcsErrorUnexpectedTpmOutput;
      case TSS_E_COMM_FAILURE:
        return kTcsErrorCommunicationFailure;
      case TSS_E_TPM_UNSUPPORTED_FEATURE:
        return kTcsErrorTpmUnsupportedFeature;
      case TCS_E_KEY_MISMATCH:
        return kTcsErrorKeyMismatch;
      case TCS_E_KM_LOADFAILED:
        return kTcsErrorKeyLoadFail;
      case TCS_E_KEY_CONTEXT_RELOAD:
        return kTcsErrorKeyContextReloadFail;
      case TCS_E_BAD_INDEX:
        return kTcsErrorBadMemoryIndex;
      case TCS_E_INVALID_CONTEXTHANDLE:
        return kTcsErrorBadContextHandle;
      case TCS_E_INVALID_KEYHANDLE:
        return kTcsErrorBadKeyHandle;
      case TCS_E_INVALID_AUTHHANDLE:
        return kTcsErrorBadAuthorizationHandle;
      case TCS_E_INVALID_AUTHSESSION:
        return kTcsErrorAuthorizationSessionClosedByTpm;
      case TCS_E_INVALID_KEY:
        return kTcsErrorInvalidKey;
      default:
        return kTcsErrorUnknownError;
    }
  } else {
    switch (TSS_ERROR_CODE(result)) {
      case TSS_E_FAIL:
        return kTssErrorGeneralFail;
      case TSS_E_BAD_PARAMETER:
        return kTssErrorBadParameter;
      case TSS_E_INTERNAL_ERROR:
        return kTssErrorInternalSoftwareError;
      case TSS_E_NOTIMPL:
        return kTssErrorNotImplemented;
      case TSS_E_PS_KEY_NOTFOUND:
        return kTssErrorKeyNotFoundInPersistentStorage;
      case TSS_E_KEY_ALREADY_REGISTERED:
        return kTssErrorKeyAlreadyRegistered;
      case TSS_E_CANCELED:
        return kTssErrorActionCanceledByRequest;
      case TSS_E_TIMEOUT:
        return kTssErrorTimeout;
      case TSS_E_OUTOFMEMORY:
        return kTssErrorOutOfMemory;
      case TSS_E_TPM_UNEXPECTED:
        return kTssErrorUnexpectedTpmOutput;
      case TSS_E_COMM_FAILURE:
        return kTssErrorCommunicationFailure;
      case TSS_E_TPM_UNSUPPORTED_FEATURE:
        return kTssErrorTpmUnsupportedFeature;
      case TSS_E_INVALID_OBJECT_TYPE:
        return kTssErrorBadObjectType;
      case TSS_E_INVALID_OBJECT_INITFLAG:
        return kTssErrorBadObjectInitFlag;
      case TSS_E_INVALID_HANDLE:
        return kTssErrorInvalidHandle;
      case TSS_E_NO_CONNECTION:
        return kTssErrorNoCoreServiceConnection;
      case TSS_E_CONNECTION_FAILED:
        return kTssErrorCoreServiceConnectionFail;
      case TSS_E_CONNECTION_BROKEN:
        return kTssErrorCoreServiceConnectionBroken;
      case TSS_E_HASH_INVALID_ALG:
        return kTssErrorInvalidHashAlgorithm;
      case TSS_E_HASH_INVALID_LENGTH:
        return kTssErrorBadHashLength;
      case TSS_E_HASH_NO_DATA:
        return kTssErrorHashObjectHasNoValue;
      case TSS_E_SILENT_CONTEXT:
        return kTssErrorSilentContextNeedsUserInput;
      case TSS_E_INVALID_ATTRIB_FLAG:
        return kTssErrorBadAttributeFlag;
      case TSS_E_INVALID_ATTRIB_SUBFLAG:
        return kTssErrorBadAttributeSubFlag;
      case TSS_E_INVALID_ATTRIB_DATA:
        return kTssErrorBadAttributeData;
      case TSS_E_NO_PCRS_SET:
        return kTssErrorNoPcrRegistersSet;
      case TSS_E_KEY_NOT_LOADED:
        return kTssErrorKeyNotLoaded;
      case TSS_E_KEY_NOT_SET:
        return kTssErrorKeyNotSet;
      case TSS_E_VALIDATION_FAILED:
        return kTssErrorValidationFailed;
      case TSS_E_TSP_AUTHREQUIRED:
        return kTssErrorTspAuthorizationRequired;
      case TSS_E_TSP_AUTH2REQUIRED:
        return kTssErrorTspMultipleAuthorizationRequired;
      case TSS_E_TSP_AUTHFAIL:
        return kTssErrorTspAuthorizationFailed;
      case TSS_E_TSP_AUTH2FAIL:
        return kTssErrorTspMultipleAuthorizationFailed;
      case TSS_E_KEY_NO_MIGRATION_POLICY:
        return kTssErrorKeyHasNoMigrationPolicy;
      case TSS_E_POLICY_NO_SECRET:
        return kTssErrorPolicyHasNoSecret;
      case TSS_E_INVALID_OBJ_ACCESS:
        return kTssErrorBadObjectAccess;
      case TSS_E_INVALID_ENCSCHEME:
        return kTssErrorBadEncryptionScheme;
      case TSS_E_INVALID_SIGSCHEME:
        return kTssErrorBadSignatureScheme;
      case TSS_E_ENC_INVALID_LENGTH:
        return kTssErrorEncryptedObjectBadLength;
      case TSS_E_ENC_NO_DATA:
        return kTssErrorEncryptedObjectHasNoData;
      case TSS_E_ENC_INVALID_TYPE:
        return kTssErrorEncryptedObjectBadType;
      case TSS_E_INVALID_KEYUSAGE:
        return kTssErrorBadKeyUsage;
      case TSS_E_VERIFICATION_FAILED:
        return kTssErrorVerificationFailed;
      case TSS_E_HASH_NO_IDENTIFIER:
        return kTssErrorNoHashAlgorithmId;
      case TSS_E_NV_AREA_EXIST:
        return kTssErrorNvRamAreaAlreadyExists;
      case TSS_E_NV_AREA_NOT_EXIST:
        return kTssErrorNvRamAreaDoesntExist;
      default:
        return kTssErrorUnknownError;
    }
  }
}

}  // namespace cryptohome