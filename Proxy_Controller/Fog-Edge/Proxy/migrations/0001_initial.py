# Generated by Django 3.1.7 on 2021-04-14 13:12

from django.db import migrations, models


class Migration(migrations.Migration):

    initial = True

    dependencies = [
    ]

    operations = [
        migrations.CreateModel(
            name='Mapper',
            fields=[
                ('id', models.AutoField(auto_created=True, primary_key=True, serialize=False, verbose_name='ID')),
                ('imsi', models.CharField(default='111', max_length=200)),
                ('loginid', models.CharField(default='a', max_length=200)),
                ('passwd', models.CharField(default='a', max_length=200)),
            ],
        ),
    ]
