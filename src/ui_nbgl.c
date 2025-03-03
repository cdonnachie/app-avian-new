/*******************************************************************************
 *   Ledger App - Bitcoin Wallet
 *   (c) 2022 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#ifdef HAVE_NBGL
#include "ui.h"
#include "nbgl_use_case.h"
#include "btchip_display_variables.h"
#include "btchip_internal.h"

#include "btchip_bagl_extensions.h"

typedef enum { 
    MESSAGE_TYPE, 
    TRANSACTION_TYPE 
} flow_type_t;


typedef struct {
  bool transaction_prompt_done;
  char *prompt_cancel_message;
  char *prompt;            // text displayed in last transaction page
  char *approved_status;   // text displayed in confirmation page (after long press)
  char *abandon_status;    // text displayed in rejection page (after reject confirmed)
  void (*approved_cb)(void);
  void (*abandon_cb)(void);
  nbgl_layoutTagValueList_t tagValueList;
  nbgl_pageInfoLongPress_t infoLongPress;
  nbgl_layoutTagValue_t tagValues[4];
  uint8_t nbPairs;
} UiContext_t;

static nbgl_page_t *pageContext;
static UiContext_t uiContext = {.transaction_prompt_done = 0};

// User action callbacks
static void approved_user_action_callback(void) {
  if (!btchip_bagl_user_action(1)) {
    ui_idle_flow();
  }
}

static void approved_user_action_processing_callback(void) {
  if (!btchip_bagl_user_action(1)) {
    nbgl_useCaseSpinner("Processing");
  }
}

static void abandon_user_action_callback(void) {
  if (!btchip_bagl_user_action(0)) {
    ui_idle_flow();
  }
}

static void approved_user_action_message_signing_callback(void) {
  btchip_bagl_user_action_message_signing(1);
  ui_idle_flow();
}

static void abandon_user_action_message_signing_callback(void) {
  btchip_bagl_user_action_message_signing(0);
  ui_idle_flow();
}

static void approved_user_action_display_processing_callback(void) {
  btchip_bagl_user_action_display(1);
  nbgl_useCaseSpinner("Processing");
}

static void approved_user_action_display_callback(void) {
  btchip_bagl_user_action_display(1);
  ui_idle_flow();
}

static void abandon_user_action_display_callback(void) {
  btchip_bagl_user_action_display(0);
  ui_idle_flow();
}

static void approved_user_action_signtx_callback(void) {
  btchip_bagl_user_action_signtx(1, 0);
  ui_idle_flow();
}

static void abandon_user_action_signtx_callback(void) {
  btchip_bagl_user_action_signtx(0, 0);
  ui_idle_flow();
}

static void releaseContext(void) {
  if (pageContext != NULL) {
    nbgl_pageRelease(pageContext);
    pageContext = NULL;
  }
}

// Status
static void abandon_status(void) {
  nbgl_useCaseStatus(uiContext.abandon_status, false, uiContext.abandon_cb);
  uiContext.transaction_prompt_done = false;
}

static void approved_status(void) {
  nbgl_useCaseStatus(uiContext.approved_status, true, uiContext.approved_cb);
  uiContext.transaction_prompt_done = false;
}

static void status_callback(bool confirm) {
  if (confirm) {
    approved_status();
  } else {
    abandon_status();
  }
}

// Prompt Cancel
static void prompt_cancel(flow_type_t type) {
  switch (type) {
      case MESSAGE_TYPE: 
          nbgl_useCaseConfirm("Reject message", "", "Yes, Reject",
                  "Go back to message", abandon_status);
          break;

      case TRANSACTION_TYPE:
          nbgl_useCaseConfirm("Reject transaction", "", "Yes, Reject",
                  "Go back to transaction", abandon_status);
          break;
  }
}

static void prompt_cancel_message(void) { 
    prompt_cancel(MESSAGE_TYPE); 
}

static void prompt_cancel_transaction(void) {
    prompt_cancel(TRANSACTION_TYPE);
}

static void transaction_review_callback(bool token) {
  if (token) {
    uiContext.approved_cb();
  } else {
    prompt_cancel_transaction();
  }
}

static void message_review_callback(bool token) {
  if (token) {
    approved_status();
  } else {
    prompt_cancel_message();
  }
}

// Continue Review
static void continue_review(flow_type_t type) {
  uiContext.tagValueList.pairs = uiContext.tagValues;
  uiContext.tagValueList.nbPairs = uiContext.nbPairs;

  uiContext.infoLongPress.icon = &G_coin_config->img_nbgl;
  uiContext.infoLongPress.longPressText = "Hold to sign";
  uiContext.infoLongPress.longPressToken = 1;
  uiContext.infoLongPress.tuneId = TUNE_TAP_CASUAL;
  uiContext.infoLongPress.text = uiContext.prompt;

  switch (type) {
      case MESSAGE_TYPE: 
          nbgl_useCaseStaticReview(&uiContext.tagValueList, &uiContext.infoLongPress,
                  "Cancel", message_review_callback);
          break;

      case TRANSACTION_TYPE:
          nbgl_useCaseStaticReview(&uiContext.tagValueList, &uiContext.infoLongPress,
                  "Cancel", transaction_review_callback);
          break;
  }
}

static void continue_message_review(void) {
    continue_review(MESSAGE_TYPE);
}

static void continue_transaction_review(void) {
  continue_review(TRANSACTION_TYPE);
}

// UI Start
static void ui_start(void (*cb)(void), flow_type_t type) {
  switch (type) {
      case MESSAGE_TYPE: 
          nbgl_useCaseReviewStart(&G_coin_config->img_nbgl, "Review\nmessage", "",
                  "Cancel", continue_message_review,
                  prompt_cancel_message);
          break;

      case TRANSACTION_TYPE:
          nbgl_useCaseReviewStart(&G_coin_config->img_nbgl, "Review\ntransaction", "",
                  "Cancel", cb, prompt_cancel_transaction);
          break;
  }
}

static void ui_transaction_start(void (*cb)(void)) {
  uiContext.abandon_status = "Transaction\nrejected";
  uiContext.approved_status = "TRANSACTION\nCONFIRMED";
  uiContext.prompt_cancel_message = "Reject\nTransaction ?";
  uiContext.prompt = "Sign transaction";
  ui_start(cb, TRANSACTION_TYPE);
}

static void ui_message_start(void) {
  uiContext.abandon_status = "Message\nrejected";
  uiContext.approved_status = "MESSAGE\nSIGNED";
  uiContext.prompt_cancel_message = "Reject\nMessage ?";
  uiContext.prompt = "Sign message";
  ui_start(NULL, MESSAGE_TYPE);
}

// Other callbacks
static void display_pubkey_callback(void) {
  if (uiContext.nbPairs == 1) {
      nbgl_useCaseAddressConfirmation(uiContext.tagValues[0].value, status_callback);
  }
  else {
      uiContext.tagValueList.pairs = &uiContext.tagValues[1];
      uiContext.tagValueList.nbPairs = 1;

      nbgl_useCaseAddressConfirmationExt(uiContext.tagValues[0].value, status_callback, &uiContext.tagValueList.pairs);
  }
}

// Flow entry point
static void single_flow_callback(int token, uint8_t index) {
  UNUSED(index);
  transaction_review_callback(token);
}

void ui_confirm_single_flow(void) {
  uiContext.approved_cb = approved_user_action_processing_callback;
  uiContext.abandon_cb = abandon_user_action_callback;

  if (!uiContext.transaction_prompt_done) {
    uiContext.transaction_prompt_done = true;

    ui_transaction_start(ui_confirm_single_flow);
  } else {
    snprintf(vars.tmp.feesAmount, sizeof(vars.tmp.feesAmount), "#%d",
             btchip_context_D.totalOutputs - btchip_context_D.remainingOutputs +
                 1);

    uiContext.tagValues[0].item = "Output";
    uiContext.tagValues[0].value = vars.tmp.feesAmount;

    uiContext.tagValues[1].item = "Amount";
    uiContext.tagValues[1].value = vars.tmp.fullAmount;

    uiContext.tagValues[2].item = "Address";
    uiContext.tagValues[2].value = vars.tmp.fullAddress;

    uiContext.nbPairs = 3;

    nbgl_pageNavigationInfo_t info = {
        .activePage =
            btchip_context_D.totalOutputs - btchip_context_D.remainingOutputs,
        .nbPages = btchip_context_D.totalOutputs - 1,
        .navType = NAV_WITH_TAP,
        .progressIndicator = true,
        .navWithTap.backButton = false,
        .navWithTap.nextPageText = "Tap to continue",
        .navWithTap.nextPageToken = 1,
        .navWithTap.quitText = "Cancel",
        .quitToken = 0,
        .tuneId = TUNE_TAP_CASUAL};

    nbgl_pageContent_t content = {
        .type = TAG_VALUE_LIST,
        .tagValueList.nbPairs = uiContext.nbPairs,
        .tagValueList.pairs = (nbgl_layoutTagValue_t *)uiContext.tagValues};
    releaseContext();
    pageContext =
        nbgl_pageDrawGenericContent(&single_flow_callback, &info, &content);
    nbgl_refresh();
  }
}

void ui_finalize_flow(void) {
  uiContext.approved_cb = approved_user_action_processing_callback;
  uiContext.abandon_cb = abandon_user_action_callback;

  uiContext.tagValues[0].item = "Fees";
  uiContext.tagValues[0].value = vars.tmp.feesAmount;

  uiContext.nbPairs = 1;

  nbgl_useCaseReviewStart(&G_coin_config->img_nbgl, "Review fees", "", "Cancel",
                          continue_transaction_review,
                          prompt_cancel_transaction);
}

void ui_request_change_path_approval_flow(void) {
  uiContext.approved_cb = approved_user_action_display_processing_callback;
  uiContext.abandon_cb = abandon_user_action_display_callback;

  if (!uiContext.transaction_prompt_done) {
    uiContext.transaction_prompt_done = true;

    ui_transaction_start(ui_request_change_path_approval_flow);
  } else {
    nbgl_useCaseChoice(&C_round_warning_64px, "Unusual\nchange path",
                       vars.tmp_warning.derivation_path, "Continue",
                       "Reject if not sure", transaction_review_callback);
  }
}

void ui_request_segwit_input_approval_flow(void) {
  uiContext.approved_cb = approved_user_action_processing_callback;
  uiContext.abandon_cb = abandon_user_action_display_callback;

  if (!uiContext.transaction_prompt_done) {
    uiContext.transaction_prompt_done = true;

    ui_transaction_start(ui_request_segwit_input_approval_flow);
  } else {
    nbgl_useCaseChoice(&C_round_warning_64px, "Unverified inputs",
                       "Update Ledger Live\nor third party software",
                       "Continue", "Reject if not sure",
                       transaction_review_callback);
  }
}

void ui_request_pubkey_approval_flow(void) {
  uiContext.approved_cb = approved_user_action_processing_callback;
  uiContext.abandon_cb = abandon_user_action_display_callback;

  if (!uiContext.transaction_prompt_done) {
    uiContext.transaction_prompt_done = true;

    ui_transaction_start(ui_request_pubkey_approval_flow);
  } else {
    nbgl_useCaseChoice(&G_coin_config->img_nbgl, "Export public key", "",
                       "Approve", "Reject", transaction_review_callback);
  }
}

void ui_request_sign_path_approval_flow(void) {
  uiContext.approved_cb = approved_user_action_signtx_callback;
  uiContext.abandon_cb = abandon_user_action_signtx_callback;

  nbgl_useCaseChoice(&C_round_warning_64px, "Unusual\nsign path",
                     vars.tmp_warning.derivation_path, "Continue",
                     "Reject if not sure", transaction_review_callback);
}

void ui_sign_message_flow(void) {
  uiContext.approved_cb = approved_user_action_message_signing_callback;
  uiContext.abandon_cb = abandon_user_action_message_signing_callback;

  uiContext.tagValues[0].item = "Message hash";
  uiContext.tagValues[0].value = vars.tmp.fullAddress;

  uiContext.nbPairs = 1;

  ui_message_start();
}

void ui_display_token_flow(void) {
  uiContext.approved_cb = approved_user_action_display_callback;
  uiContext.abandon_cb = abandon_user_action_display_callback;
  uiContext.abandon_status = "Token\nrejected";
  uiContext.approved_status = "TOKEN\nCONFIRMED";

  nbgl_useCaseChoice(&G_coin_config->img_nbgl, "Confirm token",
                     (char *)G_io_apdu_buffer + 200, "Approve", "Reject",
                     status_callback);
}

static void unusual_derivation_cb(bool status) {
  if (status) {
    display_pubkey_callback();
  } else {
    abandon_status();
  }
}
static void warn_unusual_derivation_path(void) {
  nbgl_useCaseChoice(&C_round_warning_64px, "Unusual\nderivation path", "",
                     "Continue", "Reject if not sure", unusual_derivation_cb);
}

static void prompt_public_key(bool warning) {
  if (warning) {
    nbgl_useCaseReviewStart(&G_coin_config->img_nbgl, "Confirm\npublic key", "",
                            "Cancel", warn_unusual_derivation_path,
                            abandon_status);
  } else {
    nbgl_useCaseReviewStart(&G_coin_config->img_nbgl, "Confirm\npublic key", "",
                            "Cancel", display_pubkey_callback, abandon_status);
  }
}

static void display_public_key(bool warning) {
  uiContext.abandon_status = "Public key\nrejected";
  uiContext.approved_status = "PUBLIC KEY\nCONFIRMED";
  uiContext.prompt_cancel_message = "Reject\nPublic key?";

  uiContext.tagValues[0].item = "Address";
  uiContext.tagValues[0].value = (char *)G_io_apdu_buffer + 200;

  uiContext.nbPairs = 1;

  if (warning) {
    uiContext.approved_cb = approved_user_action_callback;
    uiContext.abandon_cb = abandon_user_action_callback;

    uiContext.tagValues[1].item = "Derivation path";
    uiContext.tagValues[1].value = vars.tmp_warning.derivation_path;

    uiContext.nbPairs = 2;
  }

  else {
    uiContext.approved_cb = approved_user_action_display_callback;
    uiContext.abandon_cb = abandon_user_action_display_callback;
  }
  prompt_public_key(warning);
}

void ui_display_public_with_warning_flow(void) {
  display_public_key(true);
}

void ui_display_public_flow(void) {
  display_public_key(false);
}

void ui_transaction_finish(void) {
  if (uiContext.transaction_prompt_done) {
    uiContext.approved_status = "TRANSACTION\nCONFIRMED";
    uiContext.approved_cb = ui_idle_flow;
    approved_status();
  }
}

void ui_transaction_error(void) {
  uiContext.abandon_status = "Transaction\nerror";
  uiContext.approved_cb = ui_idle_flow;
  abandon_status();
}
#endif // HAVE_NBGL
