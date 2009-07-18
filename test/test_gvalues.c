#include <glib-object.h>

static void
string2int (const GValue *src_value,
            GValue       *dest_value)
{
  int val;
  
g_debug("%s: %s", __FUNCTION__, g_value_get_string (src_value));
  sscanf(g_value_get_string (src_value), "%d", &val);
  g_value_set_int (dest_value, val);
}

static void
int2string (const GValue *src_value,
            GValue       *dest_value)
{
  if (g_value_get_int (src_value) == 42)
    g_value_set_static_string (dest_value, "An important number");
  else
    g_value_set_static_string (dest_value, "What's that?");
}

int
main (int   argc,
      char *argv[])
{
  /* GValues must start zero-filled */
  GValue a = {0};
  GValue b = {0};
  const gchar *message;

  g_type_init ();

  /* The GValue starts empty */
  g_assert (!G_VALUE_HOLDS_STRING (&a));

  /* Put a string in it */
  g_value_init (&a, G_TYPE_STRING);
  g_assert (G_VALUE_HOLDS_STRING (&a));
  g_value_set_static_string (&a, "Hello, world!");
  g_printf ("%s\n", g_value_get_string (&a));

  /* Reset it to its pristine state */
  g_value_unset (&a);

  /* It can then be reused for another type */
  g_value_init (&a, G_TYPE_INT);
  g_value_set_int (&a, 42);

  /* Attempt to transform it into a GValue of type STRING */
  g_value_init (&b, G_TYPE_STRING);

  /* An INT is transformable to a STRING */
  g_assert (g_value_type_transformable (G_TYPE_INT, G_TYPE_STRING));

  g_value_transform (&a, &b);
  g_printf ("%s\n", g_value_get_string (&b));

  /* An INT is transformable to a STRING */
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, string2int);
  g_assert (g_value_type_transformable (G_TYPE_STRING, G_TYPE_INT));

  g_value_transform (&b, &a);
  g_printf ("%d\n", g_value_get_int (&a));

  /* Attempt to transform it again using a custom transform function */
  g_value_register_transform_func (G_TYPE_INT, G_TYPE_STRING, int2string);
  g_value_transform (&a, &b);
  g_printf ("%s\n", g_value_get_string (&b));
  return 0;
}

