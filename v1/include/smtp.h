#pragma once

int send_email(const char *recipient, const char *subject, const char *body);

int send_email_to_multiple_recipients(const char *recipients[], int num_recipients, const char *subject, const char *body);

